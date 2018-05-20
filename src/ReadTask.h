/*
 * ReadTask.h
 *
 *  Created on: 12 May 2018
 *      Author: rkfg
 */

#ifndef READTASK_H_
#define READTASK_H_

#include <unordered_map>
#include <condition_variable>
#include <libtorrent/torrent_handle.hpp>
#include <libtorrent/peer_request.hpp>

struct Piece {
    libtorrent::peer_request m_req;
    char* m_buf;
    bool ready = false;
};

class ReadTask {
public:
    ReadTask(const libtorrent::torrent_handle& handle, char *buf, int index, off_t offset,
            size_t size);
    std::mutex m_read_mutex;
    int read();
    void try_read_all();
    void try_read(int piece_idx);
    void fail(int piece_idx);
    void copy_data(int piece_idx, char *buffer, int size);
private:
    const libtorrent::torrent_handle& m_handle;
    std::unordered_map<int, Piece> m_pieces;
    int m_piece_count = 0;
    size_t m_effective_size;
    bool m_failed = false;
    std::condition_variable m_cv;

    void prioritize(int piece_idx, int priority);
    Piece* get_piece(int piece_idx);
};

#endif /* READTASK_H_ */
