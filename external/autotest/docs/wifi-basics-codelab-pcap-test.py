from os import path

golden_sequences = [['0x04', '0x05', '0x0b', '0x00', '0x01', '0x0c'],
                    ['0x04', '0x05', '0x0b', '0x01', '0x0c']]

pcap_filepath = '/tmp/filtered_pcap'
if not path.exists(pcap_filepath):
    print('Could not find output file. Follow the steps in section 5 of ' +
        'the codelab to generate a pcap output file.')
    exit()
pcap_file = open(pcap_filepath, 'r')

for line in pcap_file:
    packet_type = line[0:4]
    if not packet_type.startswith('0x0'):
        print('Failure: Non-connection packet of type ' + packet_type +
            ' included in output.')
        exit()
    for sequence in golden_sequences:
        if sequence[0] == packet_type:
            sequence.pop(0)
        if len(sequence) == 0:
            print('Success: The full connection sequence was included in ' +
                'your output!')
            exit()
pcap_file.close()

print('Failure: Your output file did not include the full connection ' +
    'sequence. You may need to add more packet types to your filter.')
exit()