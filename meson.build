project('BTFS new generation', 'cpp')
c = meson.get_compiler('cpp')
libtorrent = dependency('libtorrent-rasterbar')
fuse = dependency('fuse')
curl = dependency('libcurl')
thread_dep = dependency('threads')
boost = dependency('boost', modules : ['filesystem'])
deps = [libtorrent, fuse, curl, boost, thread_dep]
src = [
  'src/main.cpp',
  'src/ReadTask.cpp',
  'src/Session.cpp',
  'src/Torrent.cpp',
  'easyloggingpp/src/easylogging++.cc',
]

executable('btfsng', src, 
	dependencies : deps,
	cpp_args : ['-DELPP_THREAD_SAFE', '-DELPP_NO_DEFAULT_LOG_FILE']
	
)