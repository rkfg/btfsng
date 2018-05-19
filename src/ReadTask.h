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
    ReadTask(const libtorrent::torrent_handle& handle, std::mutex& read_mutex, std::condition_variable& cv, char *buf,
            int index, off_t offset, size_t size);
    int read();
    void try_read_all();
    void try_read(int piece);
    void fail(int piece_num);
    void copy_data(int piece_idx, char *buffer, int size);
private:
    const libtorrent::torrent_handle& m_handle;
    std::unordered_map<int, Piece> m_pieces;
    int m_piece_count = 0;
    size_t m_effective_size;
    bool m_failed = false;
    std::mutex& m_mutex;
    std::condition_variable& m_cv;

    void prioritize(int piece, int priority);
};

#endif /* READTASK_H_ */
