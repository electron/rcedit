{
  'targets': [
    {
      'target_name': 'rcedit',
      'type': 'executable',
      'sources': [
        'src/main.cc',
      ],
      'msvs_settings': {
        'VCLinkerTool': {
          'SubSystem': 1, # console executable
        },
        'VCCLCompilerTool': {
          'Optimization': 3, # /Ox, full optimization
          'FavorSizeOrSpeed': 1, # /Os, favour small code
        },
      },
    },
  ],
}
