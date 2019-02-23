project('BTFS new generation', 'cpp', default_options : ['cpp_std=c++14'])
c = meson.get_compiler('cpp')
add_global_arguments('-DELPP_THREAD_SAFE', '-DELPP_NO_DEFAULT_LOG_FILE', language : 'cpp')
libtorrent = dependency('libtorrent-rasterbar')
fuse = dependency('fuse')
curl = dependency('libcurl')
thread_dep = dependency('threads')
boost = dependency('boost', modules : ['filesystem'])
deps = [libtorrent, fuse, curl, boost, thread_dep, subproject('elpp').get_variable('elpp_dep')]
src = [
  'src/main.cpp',
  'src/ReadTask.cpp',
  'src/Session.cpp',
  'src/Torrent.cpp',
]

executable('btfsng', src, 
	dependencies : deps,
)