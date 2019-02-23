/*
 * compat.h
 *
 *  Created on: 23 Feb 2019
 *      Author: rkfg
 */

#ifndef COMPAT_H_
#define COMPAT_H_

#include <libtorrent/torrent_handle.hpp>
#include <libtorrent/torrent_info.hpp>
#include <boost/smart_ptr.hpp>

inline boost::shared_ptr<const libtorrent::torrent_info> torrentInfo(const libtorrent::torrent_handle& h) {
#if LIBTORRENT_VERSION_NUM < 10100
    return boost::make_shared<libtorrent::torrent_info>(h.get_torrent_info());
#else
    return h.torrent_file();
#endif
}



#endif /* COMPAT_H_ */
