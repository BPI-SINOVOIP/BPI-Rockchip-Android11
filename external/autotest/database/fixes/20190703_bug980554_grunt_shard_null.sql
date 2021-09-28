# Run against the master database cros-bighd-0001.mtv

UPDATE afe_hosts
SET leased = 0
WHERE hostname IN (
  'chromeos1-row1-rack7-host2',
  'chromeos6-row2-rack4-host4',
  'chromeos6-row2-rack4-host17',
  'chromeos6-row2-rack5-host21',
  'chromeos6-row2-rack5-host2',
  'chromeos6-row2-rack5-host1',
  'chromeos6-row2-rack5-host6',
  'chromeos6-row2-rack8-host16',
  'chromeos6-row5-rack16-host15',
  'chromeos6-row5-rack17-host17',
  'chromeos6-row5-rack18-host1',
  'chromeos6-row5-rack18-host9',
  'chromeos6-row5-rack18-host13',
  'chromeos6-row5-rack18-host15',
  'chromeos6-row5-rack19-host1',
  'chromeos6-row5-rack16-host21',
  'chromeos6-row5-rack17-host21',
  'chromeos15-row2-rack2-host5',
  'chromeos6-row2-rack4-host16',
  'chromeos6-row5-rack16-host3',
  'chromeos6-row5-rack16-host5',
  'chromeos6-row9-rack18-host17',
  'chromeos6-row9-rack18-host19',
  'chromeos6-row9-rack18-host5',
  'chromoes6-row9-rack20-host17',
  'chromoes6-row9-rack20-host21'
);
