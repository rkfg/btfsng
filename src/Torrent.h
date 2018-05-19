/*
 * Torrent.h
 *
 *  Created on: 12 May 2018
 *      Author: rkfg
 */

#ifndef TORRENT_H_
#define TORRENT_H_

#include <mutex>
#include <unordered_map>
#include <unordered_set>
#include <fuse.h>
#include <libtorrent/torrent_handle.hpp>
#include <libtorrent/add_torrent_params.hpp>
#include <libtorrent/alert_types.hpp>
#include "main.h"
#include "ReadTask.h"

class Torrent {
public:
    Torrent(std::recursive_mutex& global_mutex, btfs_params& params, libtorrent::torrent_handle& handle);
    Torrent(const Torrent& o) = delete;
    const libtorrent::torrent_handle& handle();
    void setup();
    int getattr(const char *path, struct stat *stbuf);
    int open(const char *path, struct fuse_file_info *fi);
    int read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi);
    int readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi);
    void read_piece(const libtorrent::read_piece_alert& a);
    void try_read_all(int piece);
private:
    time_t m_time_of_mount;
    std::recursive_mutex& m_global_mutex;
    btfs_params& m_params;
    libtorrent::torrent_handle m_handle;
    std::unordered_map<std::string, int> m_files;
    std::unordered_map<std::string, std::unordered_set<std::string> > m_dirs;
    std::string populate_target(char *arg);
    std::mutex m_read_mutex;
    std::condition_variable m_cv;
    std::unordered_set<std::unique_ptr<ReadTask>> m_reads;
    void populate_metadata(const std::string& arg);
    bool is_root(const char *path);
    bool is_dir(const char *path);
    bool is_file(const char *path);
};

#endif /* TORRENT_H_ */
