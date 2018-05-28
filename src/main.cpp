//============================================================================
// Name        : btfsng.cpp
// Author      : rkfg
// Version     :
// Copyright   : Your copyright notice
// Description : Hello World in C++, Ansi-style
//============================================================================

#define FUSE_USE_VERSION 26

#include <memory.h>
#include <iostream>
#include <fstream>
#include <mutex>

#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <fuse.h>
#include <curl/curl.h>
#include <libtorrent/magnet_uri.hpp>
#include <libtorrent/torrent_info.hpp>
#include <libtorrent/session.hpp>
#include "main.h"
#include "Session.h"
#include "Torrent.h"
#include "../easyloggingpp/src/easylogging++.h"
INITIALIZE_EASYLOGGINGPP

std::list<std::string> metadatas;

static struct btfs_params params;
static Session sess(params);

#define BTFS_OPT(t, p, v) { t, offsetof(struct btfs_params, p), v }

static const struct fuse_opt btfs_opts[] = {
FUSE_OPT_KEY("-v", FUSE_OPT_KEY_DISCARD),
FUSE_OPT_KEY("--v=", FUSE_OPT_KEY_DISCARD),
BTFS_OPT("--version", version, 1),
BTFS_OPT( "-h", help, 1),
BTFS_OPT("--help", help, 1),
BTFS_OPT("--help-fuse", help_fuse, 1),
BTFS_OPT("-b", browse_only, 1),
BTFS_OPT("--browse-only", browse_only, 1),
BTFS_OPT("-k", keep, 1),
BTFS_OPT("--keep", keep, 1),
BTFS_OPT( "--min-port=%lu", min_port, 4),
BTFS_OPT("--max-port=%lu", max_port, 4),
BTFS_OPT("--max-download-rate=%lu", max_download_rate, 4),
BTFS_OPT("--max-upload-rate=%lu", max_upload_rate, 4),
BTFS_OPT("-p %s", files_path, 1),
BTFS_OPT("--path=%s", files_path, 1),
FUSE_OPT_END };

std::unique_ptr<char> cwd(getcwd(NULL, 0));

static int btfs_process_arg(void *data, const char *arg, int key, struct fuse_args *outargs) {
    if (key == FUSE_OPT_KEY_NONOPT && strcmp(arg, static_cast<btfs_params*>(data)->mountpoint)) {
        metadatas.push_back(arg);
        return 0;
    }

    return 1;
}

static void print_help() {
    printf("usage: btfsng [options] metadata mountpoint\n");
    printf("\n");
    printf("btfs options:\n");
    printf("    --version              show version information\n");
    printf("    --help -h              show this message\n");
    printf("    --help-fuse            print all fuse options\n");
    printf("    --browse-only -b       download metadata only\n");
    printf("    --keep -k              keep files after unmount\n");
    printf("    --min-port=N           start of listen port range\n");
    printf("    --max-port=N           end of listen port range\n");
    printf("    --max-download-rate=N  max download rate (in kB/s)\n");
    printf("    --max-upload-rate=N    max upload rate (in kB/s)\n");
    printf("    -v, --v=N              verbose logging (1-3)\n");
}
static void* btfs_init(struct fuse_conn_info *conn) {
    try {
        chdir(cwd.get());
        sess.init();
        for (auto& metadata : metadatas) {
            sess.add_torrent(metadata);
        }
    } catch (const std::exception& e) {
        LOG(FATAL)<< "Error initializing session: " << e.what();
        fuse_exit(fuse_get_context()->fuse);
    }
    return NULL;
}

inline static int do_for_torrents(const char *path, std::function<int(const std::shared_ptr<Torrent>&)> f) {
    auto ts = sess.get_torrents_by_path(path);
    int r = 0;
    for (auto& t : ts) {
        if ((r = f(t)) < 0) {
            break;
        }
    }
    return r;
}

static int btfs_getattr(const char *path, struct stat *stbuf) {
    return do_for_torrents(path, [=](auto& t) {
        return t->getattr(path, stbuf);
    });
}

static int btfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi) {
    filler(buf, ".", NULL, 0);
    filler(buf, "..", NULL, 0);
    return do_for_torrents(path, [=](auto& t) {
        return t->readdir(path, buf, filler, offset, fi);
    });
}

static int btfs_open(const char *path, struct fuse_file_info *fi) {
    return do_for_torrents(path, [=](auto& t) {
        return t->open(path, fi);
    });
}

static int btfs_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
    return do_for_torrents(path, [=](auto& t) {
       return t->read(path, buf, size, offset, fi);
    });
}

static void btfs_destroy(void *user_data) {
    sess.stop();
}

void initLog() {
    el::Configurations defaultConf;
    defaultConf.setToDefault();
    defaultConf.setGlobally(el::ConfigurationType::Format, "%datetime %level %msg");
    defaultConf.setGlobally(el::ConfigurationType::ToStandardOutput, "true");
    defaultConf.set(el::Level::Verbose, el::ConfigurationType::Format, "%datetime %fbase:%line %level %msg");
    if (el::Loggers::verboseLevel() > 0) {
        defaultConf.setGlobally(el::ConfigurationType::Filename, "btfsng.log");
    }
    el::Loggers::addFlag(el::LoggingFlag::DisableApplicationAbortOnFatalLog);
    el::Loggers::reconfigureAllLoggers(defaultConf);
}

int main(int argc, char *argv[]) {
    START_EASYLOGGINGPP(argc, argv);
    initLog();
    struct fuse_operations btfs_ops;
    memset(&btfs_ops, 0, sizeof(btfs_ops));
    btfs_ops.init = btfs_init;
    btfs_ops.getattr = btfs_getattr;
    btfs_ops.readdir = btfs_readdir;
    btfs_ops.open = btfs_open;
    btfs_ops.read = btfs_read;
    btfs_ops.destroy = btfs_destroy;
    struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
    params.mountpoint = argv[argc - 1];
    if (fuse_opt_parse(&args, &params, btfs_opts, btfs_process_arg)) {
        LOG(FATAL)<< "Failed to parse options";
        return 1;
    }

    if (metadatas.empty()) {
        params.help = 1;
    }

    if (params.version) {
        printf("btfsng version: 0.1\n");
        printf("libtorrent version: " LIBTORRENT_VERSION "\n");

        // Let FUSE print more versions
        fuse_opt_add_arg(&args, "--version");
        fuse_main(args.argc, args.argv, &btfs_ops, NULL);

        return 0;
    }

    if (params.help || params.help_fuse) {
        // Print info about btfs' command line options
        print_help();

        if (params.help_fuse) {
            printf("\n");

            // Let FUSE print more help
            fuse_opt_add_arg(&args, "-ho");
            fuse_main(args.argc, args.argv, &btfs_ops, NULL);
        }

        return 0;
    }

    if (params.min_port == 0 && params.max_port == 0) {
        // Default ports are the standard Bittorrent range
        params.min_port = 6881;
        params.max_port = 6889;
    } else if (params.min_port == 0) {
        params.min_port = 1024;
    } else if (params.max_port == 0) {
        params.max_port = 65535;
    }

    if (params.min_port > params.max_port) {
        LOG(FATAL)<< "Invalid port range";
        return 1;
    }

    curl_global_init(CURL_GLOBAL_ALL);

    fuse_main(args.argc, args.argv, &btfs_ops, NULL);

    curl_global_cleanup();

    return 0;
}
