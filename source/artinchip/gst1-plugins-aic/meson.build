project('gst1.0-plugin-aic', 'c',
  version : '0.7.0',
  meson_version : '>= 0.48',
)

api_version = '1.0'

gst_req = '>= 1.6.0'

pro_version = meson.project_version()
pro_name = meson.project_name()

cc = meson.get_compiler('c')

add_project_link_arguments('-g', language : 'c')

plugins_install_dir = join_paths(get_option('libdir'), 'gstreamer-1.0')

mpp_dep = cc.find_library('mpp_decoder')
mpp_base_dep = cc.find_library('mpp_base')
mpp_ve_dep = cc.find_library('mpp_ve')
gst_dep = dependency('gstreamer-' + api_version, version : gst_req)
gst_base_dep = dependency('gstreamer-base-' + api_version, version : gst_req)
gst_allocator_dep = dependency('gstreamer-allocators-' + api_version, version : gst_req)
gst_plugins_base_dep = dependency('gstreamer-plugins-base-' + api_version, version : gst_req)
gst_video_dep = dependency('gstreamer-video-' + api_version, version : gst_req)

gstaic_sources = [
	'gstvedec.c',
	'gstfbsink.c',
	'gstplugin.c',
	'gstaicfb.c',
	'gstmppallocator.c',
]

gstaic_headers = [
	'gstaicfb.h',
	'gstvedec.h',
	'gstfbsink.h',
	'gstmppallocator.h',
]

gstaic = library('gstaic',
  gstaic_sources + gstaic_headers,
  dependencies : [gst_dep, gst_base_dep, gst_plugins_base_dep, gst_video_dep, mpp_dep, mpp_base_dep, mpp_ve_dep],
  install : true,
  install_dir : plugins_install_dir,
)

plugins = [gstaic]
