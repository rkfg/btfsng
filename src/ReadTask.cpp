/*
 * ReadTask.cpp
 *
 *  Created on: 12 May 2018
 *      Author: rkfg
 */

#include "ReadTask.h"
#include <libtorrent/torrent_info.hpp>

void ReadTask::prioritize(int piece) {
    if (!m_handle.have_piece(piece)) {
        std::cout << "Prioritizing piece " << piece << std::endl;
        m_handle.piece_priority(piece, 7);
    }
}

ReadTask::ReadTask(const libtorrent::torrent_handle& handle, std::mutex& read_mutex, std::condition_variable& cv,
        char *buf, int index, off_t offset, size_t size) :
        m_handle(handle), m_mutex(read_mutex), m_cv(cv) {
    std::cout << "New read index=" << index << " offset=" << offset << " size=" << size << std::endl;
    auto ti = m_handle.torrent_file();

    int64_t file_size = ti->files().file_size(index);

    m_effective_size = 0;
    int last_piece = 0;
    while (size > 0 && offset < file_size) {
        libtorrent::peer_request req = ti->map_file(index, offset, (int) size);

        req.length = std::min(ti->piece_size(req.piece) - req.start, req.length);

        std::cout << "Adding piece " << req.piece << " len=" << req.length << std::endl;
        if (!m_pieces.emplace(req.piece, Piece { req, buf }).second) {
            std::cout << "Not adding piece " << req.piece << " len=" << req.length << " as it's already present!" << std::endl;
        }

        size -= (size_t) req.length;
        offset += req.length;
        buf += req.length;
        m_effective_size += req.length;
        ++m_piece_count;
        last_piece = req.piece;
        prioritize(last_piece);
    }
    for (int i = 1; i < 16 - m_piece_count + 1; ++i) {
        int piece = last_piece + i;
        prioritize(piece);
    }
}

int ReadTask::read() {
    if (m_effective_size <= 0)
        return 0;

    try_read_all();

    std::unique_lock<std::mutex> lock(m_mutex);
    m_cv.wait(lock, [this] {
        return !m_piece_count || m_failed;
    });

    if (m_failed)
        return -EIO;
    else
        return m_effective_size;
}

void ReadTask::try_read_all() {
    for (auto& p : m_pieces) {
        if (m_handle.have_piece(p.first))
            m_handle.read_piece(p.first);
    }
}

void ReadTask::fail(int piece_num) {
    auto p = m_pieces.find(piece_num);
    if (p == m_pieces.end() || p->second.ready) {
        return;
    }
    m_failed = true;
}

void ReadTask::try_read(int piece) {
    std::cout << "Try read " << piece << std::endl;
    auto p = m_pieces.find(piece);
    if (p == m_pieces.end()) {
        std::cout << "Not interested in " << piece << std::endl;
        return;
    }
    std::cout << "Sent read request for piece " << piece << std::endl;
    m_handle.read_piece(piece);
}

void ReadTask::copy_data(int piece_idx, char *buffer, int size) {
    std::cout << "Want to copy data idx=" << piece_idx << " size=" << size << std::endl;
    auto p = m_pieces.find(piece_idx);
    if (p == m_pieces.end()) {
        std::cout << "Don't have idx=" << piece_idx << " size=" << size << std::endl;
        return;
    }
    auto& piece = p->second;
    if (!piece.ready) {
        std::cout << "Data ready, copying idx=" << piece_idx << " offset=" << piece.m_req.start << " size="
                << piece.m_req.length << std::endl;
        piece.ready = (memcpy(piece.m_buf, buffer + piece.m_req.start, (size_t) piece.m_req.length)) != NULL;
        --m_piece_count;
    } else {
        std::cout << "Data is already copided idx=" << piece_idx << " size=" << piece.m_req.length << std::endl;
    }
}
