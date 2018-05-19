/*
 * main.h
 *
 *  Created on: 12 May 2018
 *      Author: rkfg
 */

#ifndef MAIN_H_
#define MAIN_H_

#include <list>

#define LOCK std::lock_guard<std::recursive_mutex> l(m_global_mutex)

struct btfs_params {
    int version;
    int help;
    int help_fuse;
    int browse_only;
    int keep;
    int min_port;
    int max_port;
    int max_download_rate;
    int max_upload_rate;
};

#endif /* MAIN_H_ */
