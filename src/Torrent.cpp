/*
 * Torrent.cpp
 *
 *  Created on: 12 May 2018
 *      Author: rkfg
 */

#include "Torrent.h"
#include <curl/curl.h>
#include <libtorrent/torrent_info.hpp>
#include <libtorrent/magnet_uri.hpp>
#include "easylogging++.h"

#define LOCK_TORRENT std::lock_guard<std::recursive_mutex> l(m_mutex)

Torrent::Torrent(btfs_params& params, libtorrent::torrent_handle& handle) :
        m_params(params), m_handle(handle) {
    m_time_of_mount = time(NULL);
}

const libtorrent::torrent_handle& Torrent::handle() {
    return m_handle;
}

bool Torrent::is_root(const char *path) {
    return strcmp(path, "/") == 0;
}

bool Torrent::is_dir(const char *path) {
    return m_dirs.find(path) != m_dirs.end();
}

bool Torrent::has_path(const char* path) {
    return is_dir(path) || is_file(path);
}

bool Torrent::is_file(const char *path) {
    return m_files.find(path) != m_files.end();
}

int Torrent::getattr(const char *path, struct stat *stbuf) {
    if (!is_dir(path) && !is_file(path) && !is_root(path))
        return -ENOENT;

    memset(stbuf, 0, sizeof(*stbuf));

    stbuf->st_uid = getuid();
    stbuf->st_gid = getgid();
    stbuf->st_mtime = m_time_of_mount;

    if (is_root(path) || is_dir(path)) {
        stbuf->st_mode = S_IFDIR | 0555;
    } else {
        auto ti = m_handle.torrent_file();

        int64_t file_size = ti->files().file_size(m_files[path]);

        std::vector<boost::int64_t> progress;

        // Get number of bytes downloaded of each file
        m_handle.file_progress(progress, libtorrent::torrent_handle::piece_granularity);

        stbuf->st_blocks = progress[(size_t) m_files[path]] / 512;
        stbuf->st_mode = S_IFREG | 0444;
        stbuf->st_size = file_size;
    }

    return 0;
}

int Torrent::open(const char* path, struct fuse_file_info* fi) {
    if (!is_dir(path) && !is_file(path)) {
        return -ENOENT;
    }

    if (is_dir(path)) {
        return -EISDIR;
    }

    if ((fi->flags & 3) != O_RDONLY) {
        return -EACCES;
    }

    return 0;
}

int Torrent::read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
    if (!is_dir(path) && !is_file(path)) {
        return -ENOENT;
    }

    if (is_dir(path)) {
        return -EISDIR;
    }

    if (m_params.browse_only) {
        return -EACCES;
    }

    m_mutex.lock(); // only lock torrent's mutex to add and remove pending reads to avoid races
    auto& r = *m_reads.emplace(std::make_unique<ReadTask>(m_handle, buf, m_files[path], offset, size)).first;
    m_mutex.unlock();

    // Wait for read to finish
    int s = r->read();

    m_mutex.lock();
    m_reads.erase(r);
    m_mutex.unlock();
    return s;
}

int Torrent::readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi) {
    if (!is_dir(path) && !is_file(path) && !is_root(path))
        return -ENOENT;

    if (is_file(path))
        return -ENOTDIR;

    for (auto& d : m_dirs[path]) {
        filler(buf, d.c_str(), NULL, 0);
    }

    return 0;
}

void Torrent::setup() {
    VLOG(1) << "Got metadata. Now ready to start downloading.";

    auto ti = m_handle.torrent_file();

    if (m_params.browse_only)
        m_handle.pause();

    for (int i = 0; i < ti->num_files(); ++i) {
        std::string parent("");

        std::unique_ptr<char> p(strdup(ti->files().file_path(i).c_str()));

        if (!p)
            continue;

        char *ptr;
        for (char *x = strtok_r(p.get(), "/", &ptr); x; x = strtok_r(NULL, "/", &ptr)) {
            if (strlen(x) <= 0)
                continue;

            if (parent.length() <= 0)
                // Root dir <-> children mapping
                m_dirs["/"].insert(x);
            else
                // Non-root dir <-> children mapping
                m_dirs[parent].insert(x);

            parent += "/";
            parent += x;
        }

        // Path <-> file index mapping
        m_files["/" + ti->files().file_path(i)] = i;
    }
}

void Torrent::read_piece(const libtorrent::read_piece_alert& a) {
    LOCK_TORRENT;
    VLOG(3) << "Read piece " << a.piece;
    if (a.ec) {
        LOG(WARNING)<< a.message();
        for (auto& r : m_reads) {
            r->fail(a.piece);
        }
    } else {
        for (auto& r : m_reads) {
            r->copy_data(a.piece, a.buffer.get(), a.size);
        }
    }
}

void Torrent::try_read_all(int piece) {
    LOCK_TORRENT;
    for (auto& r : m_reads) {
        r->try_read(piece);
    }
}
