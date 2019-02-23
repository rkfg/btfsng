/*
 * Session.cpp
 *
 *  Created on: 12 May 2018
 *      Author: rkfg
 */

#include "Session.h"
#include <libtorrent/alert_types.hpp>
#include <libtorrent/magnet_uri.hpp>
#include <libtorrent/torrent_info.hpp>
#include <boost/filesystem.hpp>
#include <curl/curl.h>
#include "easylogging++.h"
#define STRINGIFY(s) #s

#define LOCK_SESSION std::lock_guard<std::recursive_mutex> l(m_mutex)

Session::Session(btfs_params& params) :
        m_params(params) {
}

void Session::stop() {
    m_stop = true;
#if LIBTORRENT_VERSION_NUM < 10200
    int flags = 0;
#else
    libtorrent::remove_flags_t flags = {};
#endif
    {
        LOCK_SESSION;
        if (!m_params.keep)
        flags |= libtorrent::session::delete_files;

        if (m_session) {
            for (auto& t : m_thmap) {
                m_session->remove_torrent(t.second->handle(), flags);
            }
        }
    }
    try {
        if (m_alert_thread && m_alert_thread->joinable()) { // race condition is possible here, will be caught
            m_alert_thread->join();
        }
    } catch (const std::exception& e) {
        LOG(WARNING)<< "Couldn't join alert thread: " << e.what();
    }
    if (!m_params.keep) {
        for (auto& t : m_thmap) {
            namespace fs = boost::filesystem;
            fs::path save_path(t.first.status().save_path);
            fs::remove_all(save_path);
        }
    }
}

void Session::init() {

#if LIBTORRENT_VERSION_NUM < 10200
    int flags =
#else
            libtorrent::session_flags_t flags =
#endif
            libtorrent::session::add_default_plugins | libtorrent::session::start_default_features;

#if LIBTORRENT_VERSION_NUM < 10200
    int alerts =
#else
            libtorrent::alert_category_t alerts =
#endif
            libtorrent::alert::tracker_notification | libtorrent::alert::stats_notification
                    | libtorrent::alert::storage_notification | libtorrent::alert::progress_notification
                    | libtorrent::alert::status_notification | libtorrent::alert::error_notification
                    | libtorrent::alert::dht_notification | libtorrent::alert::peer_notification;

#if LIBTORRENT_VERSION_NUM < 10100
    m_session = new libtorrent::session(
            libtorrent::fingerprint(
                    "LT",
                    LIBTORRENT_VERSION_MAJOR,
                    LIBTORRENT_VERSION_MINOR,
                    0,
                    0),
            std::make_pair(m_params.min_port, m_params.max_port),
            "0.0.0.0",
            flags,
            alerts);

    libtorrent::session_settings se = m_session->settings();

    se.request_timeout = 10;
    se.strict_end_game_mode = false;
    se.announce_to_all_trackers = true;
    se.announce_to_all_tiers = true;
    se.download_rate_limit = m_params.max_download_rate * 1024;
    se.upload_rate_limit = m_params.max_upload_rate * 1024;

    m_session->set_settings(se);
    m_session->add_dht_router(std::make_pair("router.bittorrent.com", 6881));
    m_session->add_dht_router(std::make_pair("router.utorrent.com", 6881));
    m_session->add_dht_router(std::make_pair("dht.transmissionbt.com", 6881));
#else
    libtorrent::settings_pack pack;

    std::ostringstream interfaces;

// First port
    interfaces << "0.0.0.0:" << m_params.min_port;

// Possibly more ports, but at most 5
    for (int i = m_params.min_port + 1; i <= m_params.max_port && i < m_params.min_port + 5; i++)
        interfaces << ",0.0.0.0:" << i;

    std::string fingerprint = "LT"
    STRINGIFY(LIBTORRENT_VERSION_MAJOR)
    STRINGIFY(LIBTORRENT_VERSION_MINOR)
    "00";

#if LIBTORRENT_VERSION_NUM >= 10101
    pack.set_str(pack.dht_bootstrap_nodes, "router.bittorrent.com:6881,"
            "router.utorrent.com:6881,"
            "dht.transmissionbt.com:6881");
#endif

    pack.set_int(pack.request_timeout, 10);
    pack.set_str(pack.listen_interfaces, interfaces.str());
    pack.set_bool(pack.strict_end_game_mode, false);
    pack.set_bool(pack.announce_to_all_trackers, true);
    pack.set_bool(pack.announce_to_all_tiers, true);
    pack.set_int(pack.download_rate_limit, m_params.max_download_rate * 1024);
    pack.set_int(pack.upload_rate_limit, m_params.max_upload_rate * 1024);
    pack.set_int(pack.alert_mask, alerts);

    m_session = std::make_unique<libtorrent::session>(pack, flags);

#if LIBTORRENT_VERSION_NUM < 10101
    m_session->add_dht_router(std::make_pair("router.bittorrent.com", 6881));
    m_session->add_dht_router(std::make_pair("router.utorrent.com", 6881));
    m_session->add_dht_router(std::make_pair("dht.transmissionbt.com", 6881));
#endif

#endif
    m_alert_thread = std::make_unique<std::thread>(&Session::alert_queue_loop, this);
}

void Session::alert_queue_loop() {
    VLOG(1) << "Alert thread started";
    while (!m_stop) {
        if (!m_session->wait_for_alert(libtorrent::seconds(1)))
            continue;

#if LIBTORRENT_VERSION_NUM < 10100
        std::deque<libtorrent::alert*> alerts;

        m_session->pop_alerts(&alerts);

        for (std::deque<libtorrent::alert*>::iterator i = alerts.begin(); i != alerts.end(); ++i) {
            handle_alert(*i, (Log *) data);
        }
#else
        std::vector<libtorrent::alert*> alerts;

        {
            LOCK_SESSION;
            m_session->pop_alerts(&alerts);

            for (auto& alert : alerts) {
                handle_alert(alert);
            }
        }
#endif
    }
}

Torrent& Session::add_torrent(const std::string& metadata) {
    LOCK_SESSION;
    VLOG(1) << "Adding torrent from " << metadata;
    auto handle = m_session->add_torrent(create_torrent_params(metadata));
    auto res = m_thmap.emplace(handle, std::make_unique<Torrent>(m_params, handle));
    return *res.first->second;
}

std::list<std::shared_ptr<Torrent>> Session::get_torrents_by_path(const char* path) {
    std::list<std::shared_ptr<Torrent>> result;
    bool all = !strcmp(path, "/");
    for (auto& t : m_thmap) {
        if (all || t.second->has_path(path)) {
            result.emplace_back(t.second);
        }
    }
    return result;
}

void Session::handle_torrent_added_alert(libtorrent::torrent_added_alert *a, Torrent& t) {
    VLOG(1) << "Torrent '" << a->handle.status().name << "' added";
    if (a->handle.status().has_metadata) {
        t.setup();
    }
}

void Session::handle_metadata_received_alert(libtorrent::metadata_received_alert *a, Torrent& t) {
    VLOG(1) << "Metadata for '" << a->handle.status().name << "' received";
    t.setup();
}

void Session::handle_read_piece_alert(libtorrent::read_piece_alert *a, Torrent& t) {
    VLOG(2) << "Piece " << a->piece << " read";
    t.read_piece(*a);
}

void Session::handle_piece_finished_alert(libtorrent::piece_finished_alert *a, Torrent& t) {
    VLOG(2) << "Piece " << a->piece_index << " finished downloading";
    t.try_read_all(a->piece_index);
}
void Session::handle_alert(libtorrent::alert *a) {
    decltype(m_thmap)::iterator t;
    libtorrent::torrent_alert* ta = dynamic_cast<libtorrent::torrent_alert*>(a);
    if (ta) {
        if ((t = m_thmap.find(ta->handle)) == m_thmap.end()) {
            return;
        }
    }
    switch (a->type()) {
    case libtorrent::read_piece_alert::alert_type:
        handle_read_piece_alert((libtorrent::read_piece_alert *) a, *t->second);
        break;
    case libtorrent::piece_finished_alert::alert_type:
        handle_piece_finished_alert((libtorrent::piece_finished_alert *) a, *t->second);
        break;
    case libtorrent::metadata_received_alert::alert_type:
        handle_metadata_received_alert((libtorrent::metadata_received_alert *) a, *t->second);
        break;
    case libtorrent::torrent_added_alert::alert_type:
        handle_torrent_added_alert((libtorrent::torrent_added_alert *) a, *t->second);
        break;
    case libtorrent::dht_bootstrap_alert::alert_type:
        // Force DHT announce because libtorrent won't by itself
        for (auto& t : m_thmap) {
            t.second->handle().force_dht_announce();
        }
        break;
    case libtorrent::dht_announce_alert::alert_type:
    case libtorrent::dht_reply_alert::alert_type:
    case libtorrent::metadata_failed_alert::alert_type:
    case libtorrent::tracker_announce_alert::alert_type:
    case libtorrent::tracker_reply_alert::alert_type:
    case libtorrent::tracker_warning_alert::alert_type:
    case libtorrent::tracker_error_alert::alert_type:
    case libtorrent::lsd_peer_alert::alert_type:
        break;
    case libtorrent::stats_alert::alert_type:
        break;
    default:
        break;
    }

#if LIBTORRENT_VERSION_NUM < 10100
    delete a;
#endif
}

libtorrent::add_torrent_params Session::create_torrent_params(const std::string& metadata) {
    libtorrent::add_torrent_params add_params;
    std::string target = populate_target();
    VLOG(1) << "Files path: " << target;

#if LIBTORRENT_VERSION_NUM < 10200
    add_params.flags &= ~libtorrent::add_torrent_params::flag_auto_managed;
    add_params.flags &= ~libtorrent::add_torrent_params::flag_paused;
#else
    add_params.flags &= ~libtorrent::torrent_flags::auto_managed;
    add_params.flags &= ~libtorrent::torrent_flags::paused;
#endif
    add_params.save_path = target;

    populate_metadata(metadata, add_params);
    return add_params;
}

inline void create_directory(const std::string& dir) {
    try {
        boost::filesystem::create_directories(dir);
    } catch (const boost::filesystem::filesystem_error& e) {
        throw std::runtime_error(std::string("Failed to create target: ") + e.what());
    }
}

inline std::string expand(const char* dir) {
    std::unique_ptr<char> x(realpath(dir, NULL));

    if (x)
        return x.get();
    else
        throw std::runtime_error("Failed to expand target");
}

std::string Session::populate_target() {
    std::string templ, target;

    if (m_params.files_path != NULL) {
        templ = m_params.files_path + std::string("/files");
        create_directory(templ);
        return expand(templ.c_str());
    } else if (getenv("HOME")) {
        templ += getenv("HOME");
        templ += "/.local/share/btfsng";
    } else {
        templ += "/tmp/btfsng";
    }

    create_directory(templ);

    templ += "/btfsng-XXXXXX";

    std::unique_ptr<char> s(strdup(templ.c_str()));

    if (s != NULL && mkdtemp(s.get()) != NULL) {
        return expand(s.get());
    } else {
        throw std::runtime_error("Failed to generate target");
    }
}

size_t handle_http(void *contents, size_t size, size_t nmemb, void *userp) {
    std::vector<char>& http_response = *(std::vector<char>*) userp;
    // Offset into buffer to write to
    size_t off = http_response.size();

    http_response.resize(nmemb * size + off);

    memcpy(http_response.data() + off, contents, nmemb * size);

    // Must return number of bytes copied
    return nmemb * size;
}

void Session::populate_metadata(const std::string& uri, libtorrent::add_torrent_params& params) {

    VLOG(1) << "Trying to populate metadata from " << uri;
    if (uri.find("http:") == 0 || uri.find("https:") == 0) {
        CURL *ch = curl_easy_init();

        std::vector<char> http_response;
        curl_easy_setopt(ch, CURLOPT_URL, uri.c_str());
        curl_easy_setopt(ch, CURLOPT_WRITEFUNCTION, &handle_http);
        curl_easy_setopt(ch, CURLOPT_WRITEDATA, &http_response);
        curl_easy_setopt(ch, CURLOPT_USERAGENT, "btfsng/0.1");
        curl_easy_setopt(ch, CURLOPT_FOLLOWLOCATION, 1);

        VLOG(1) << "http(s) metadata needed, downloading";
        CURLcode res = curl_easy_perform(ch);

        if (res != CURLE_OK)
            throw std::runtime_error(std::string("Download metadata failed: ") + curl_easy_strerror(res));

        curl_easy_cleanup(ch);

        libtorrent::error_code ec;

#if LIBTORRENT_VERSION_NUM < 10100
        params.ti = new libtorrent::torrent_info((const char *) http_response.buf, (int) http_response.size, ec);
#elif LIBTORRENT_VERSION_NUM < 10200
        params.ti = boost::make_shared<libtorrent::torrent_info>((const char *) http_response.data(),
                (int) http_response.size(), boost::ref(ec));
#else
        params.ti = std::make_shared<libtorrent::torrent_info>(
                (const char *) m_http_response.buf, (int) m_http_response.size,
                std::ref(ec));
#endif

        if (ec)
            throw std::runtime_error(std::string("Parse metadata failed: ") + ec.message());

        if (m_params.browse_only)
#if LIBTORRENT_VERSION_NUM < 10200
            params.flags |= libtorrent::add_torrent_params::flag_paused;
#else
        params.flags |= libtorrent::torrent_flags::paused;
#endif
    } else if (uri.find("magnet:") == 0) {
        VLOG(1) << "Magnet metadata needed, requesting";
        libtorrent::error_code ec;

        parse_magnet_uri(uri, params, ec);

        if (ec)
            throw std::runtime_error(std::string("Parse magnet failed: " + ec.message()));
    } else {
        VLOG(1) << "File needed, reading";
        std::unique_ptr<char> r(realpath(uri.c_str(), NULL));

        if (!r)
            throw std::runtime_error("Metadata file not found");

        libtorrent::error_code ec;

#if LIBTORRENT_VERSION_NUM < 10100
        params.ti = new libtorrent::torrent_info(r.get(), ec);
#elif LIBTORRENT_VERSION_NUM < 10200
        params.ti = boost::make_shared<libtorrent::torrent_info>(r.get(), boost::ref(ec));
#else
        params.ti = std::make_shared<libtorrent::torrent_info>(r.get(),
                std::ref(ec));
#endif

        if (ec)
            throw std::runtime_error(std::string("Parse metadata failed: " + ec.message()));

        if (m_params.browse_only)
#if LIBTORRENT_VERSION_NUM < 10200
            params.flags |= libtorrent::add_torrent_params::flag_paused;
#else
        params.flags |= libtorrent::torrent_flags::paused;
#endif
    }

}

Session::~Session() {
    stop();
}
