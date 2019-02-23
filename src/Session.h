/*
 * Session.h
 *
 *  Created on: 12 May 2018
 *      Author: rkfg
 */

#ifndef SESSION_H_
#define SESSION_H_

#include <fuse.h>
#include <mutex>
#include <thread>
#include <boost/unordered_map.hpp>
#include "Torrent.h"
#include <libtorrent/session.hpp>
#include <libtorrent/alert_types.hpp>
#include "main.h"

class Session {
public:
    Session(btfs_params& params);
    void init();
    void stop();
    Torrent& add_torrent(const std::string& metadata);
    std::list<std::shared_ptr<Torrent>> get_torrents_by_path(const char* path);
    ~Session();
private:
    std::recursive_mutex m_mutex;
    btfs_params& m_params;
    std::unique_ptr<libtorrent::session> m_session;
    std::unique_ptr<std::thread> m_alert_thread;
    bool m_stop = false;
    boost::unordered_map<libtorrent::torrent_handle, std::shared_ptr<Torrent>> m_thmap;
    void alert_queue_loop();
    void handle_alert(libtorrent::alert *a);
    void handle_torrent_added_alert(libtorrent::torrent_added_alert *a, Torrent& t);
    void handle_metadata_received_alert(libtorrent::metadata_received_alert *a, Torrent& t);
    void handle_read_piece_alert(libtorrent::read_piece_alert *a, Torrent& t);
    void handle_piece_finished_alert(libtorrent::piece_finished_alert *a, Torrent& t);
    libtorrent::add_torrent_params create_torrent_params(const std::string& metadata);
    std::string populate_target();
    void populate_metadata(const std::string& uri, libtorrent::add_torrent_params& params);
};

#endif /* SESSION_H_ */
