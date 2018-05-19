# BTFSNG (bittorrent filesystem, new generation)

## What is this?

With BTFSNG, you can mount any **.torrent** file or **magnet link** and then use it as any read-only directory in your file tree. The contents of the files will be downloaded on-demand as they are read by applications. Tools like **ls**, **cat** and **cp** works as expected. Applications like **vlc** and **mplayer** can also work without changes.

## What are the differences from BTFS?

This is a heavily refactored version of [BTFS](https://github.com/johang/btfs).

- the build system is replaced with [Meson](https://mesonbuild.com/)
- most of the code is moved into classes
- global variables and static functions mostly removed
- some optimizations are made (replaced maps with unordered maps, implemented more precise pieces triggers etc.)
- pthread function calls are replaced with C++11 synchronization primitives and threads.

In future multitorrent support might be introduced.

## Example usage

    $ mkdir mnt
    $ btfsng video.torrent mnt
    $ cd mnt
    $ vlc video.mp4

To unmount and shutdown:

    $ fusermount -u mnt

## Dependencies (on Linux)

* fuse ("fuse" in Ubuntu 16.04)
* libtorrent ("libtorrent-rasterbar8" in Ubuntu 16.04)
* libcurl ("libcurl3" in Ubuntu 16.04)
* boost-system, boost-filesystem ("libboost-filesystem" and "libboost-system-dev" in Ubuntu 16.04)

## Building from git on a recent Debian/Ubuntu

    $ sudo apt-get install autoconf automake libfuse-dev libtorrent-rasterbar-dev libcurl4-openssl-dev libboost-filesystem-dev g++ meson
    $ git clone https://github.com/rkfg/btfsng.git btfsng
    $ cd btfsng
    $ meson build
    $ cd build
    $ ninja

And optionally, if you want to install it:

    $ ninja install

## Building on macOS

Use [`brew`](https://brew.sh) to get the dependencies.

    $ brew install Caskroom/cask/osxfuse libtorrent-rasterbar libboost-filesystem-dev autoconf automake pkg-config meson
    $ git clone https://github.com/rkfg/btfsng.git btfsng
    $ cd btfsng
    $ meson build
    $ cd build
    $ ninja

And optionally, if you want to install it:

    $ ninja install
