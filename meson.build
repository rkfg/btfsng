project('BTFS new generation', 'cpp')
c = meson.get_compiler('cpp')
libtorrent = dependency('libtorrent-rasterbar')
fuse = dependency('fuse')
curl = dependency('libcurl')
thread_dep = dependency('threads')
#boost = dependency('boost', modules : ['system', 'coroutine', 'program_options'])
deps = [libtorrent, fuse, curl, thread_dep]
src = [
  'src/main.cpp',
  'src/ReadTask.cpp',
  'src/Session.cpp',
  'src/Torrent.cpp',
]

executable('btfsng', src, 
	dependencies : deps,
)