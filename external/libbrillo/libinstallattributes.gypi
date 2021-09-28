{
  'targets': [
    {
      'target_name': 'libinstallattributes-includes',
      'type': 'none',
      'copies': [
        {
          'destination': '<(SHARED_INTERMEDIATE_DIR)/include/install_attributes',
          'files': [
            'install_attributes/libinstallattributes.h',
          ],
        },
      ],
    },
  ],
}
