gen_hw_metrics = [
  'hsw',
  'bdw', 'chv',
  'sklgt2', 'sklgt3', 'sklgt4',
  'kblgt2', 'kblgt3',
  'cflgt2', 'cflgt3',
  'bxt', 'glk',
  'cnl',
  'icl',
  'tgl',
]

gen_hw_metrics_xml_files = []
foreach hw : gen_hw_metrics
  gen_hw_metrics_xml_files += 'oa-@0@.xml'.format(hw)
endforeach

gen_perf_sources = [
  'gen_perf.c',
  'gen_perf_mdapi.c',
]

gen_perf_sources += custom_target(
  'intel-perf-sources',
  input : gen_hw_metrics_xml_files,
  output : [ 'gen_perf_metrics.c', 'gen_perf_metrics.h' ],
  command : [
    prog_python, files('gen_perf.py'),
    '--code', '@OUTPUT0@', '--header', '@OUTPUT1@',
    '@INPUT@',
  ],
)

libintel_perf = static_library(
  'intel_perf',
  gen_perf_sources,
  include_directories : [
    inc_common, inc_intel, inc_util,
  ],
  c_args : [c_vis_args, no_override_init_args, '-msse2'],
  cpp_args : [cpp_vis_args, '-msse2'],
)
