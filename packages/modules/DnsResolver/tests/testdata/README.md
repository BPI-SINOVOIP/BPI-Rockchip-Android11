
# Resolv Gold Test
The "Resolv Gold Test" targets to run automatically in presubmit, as a change
detector to ensure that the resolver doesn't send the query or parse the
response unexpectedly.

## Build testing pbtext
The testing pbtext is built manually so far. Fill expected API parameters to
'config' and expected answers to 'result' in pbtext. Then, record the
corresponding DNS query and response packets. Fill the packets with the \x
formatting into 'query' and 'response' in pbtext. Perhaps have a mechanism
to generate the pbtxt automatically in the future.

### Using 'ping' utility to be an example for building pbtext
Here demonstrates how the pbtext is built.

1. Enable resolver debug log level to VERBOSE (0)
```
$ adb shell service call dnsresolver 10 i32 0
```
2. Ping a website
```
$ adb shell ping www.google.com
```
3. Get bugreport
```
$ adb bugreport
```
4. Search the log pattern as the follows in bugreport
```
# API arguments
resolv  : logArguments: argv[0]=gethostbyname
resolv  : logArguments: argv[1]=0
resolv  : logArguments: argv[2]=www.google.com
resolv  : logArguments: argv[3]=2

# Query packet
resolv  : ;; ->>HEADER<<- opcode: QUERY, status: NOERROR, id: 29827
resolv  : ;; flags: rd; QUERY: 1, ANSWER: 0, AUTHORITY: 0, ADDITIONAL: 0
resolv  : ;; QUERY SECTION:
resolv  : ;;    www.google.com, type = A, class = IN
resolv  :
resolv  : Hex dump:
resolv  : 7483010000010000000000000377777706676f6f676c6503636f6d0000010001

# Response packet
resolv  : ;; ->>HEADER<<- opcode: QUERY, status: NOERROR, id: 29827
resolv  : ;; flags: qr rd ra; QUERY: 1, ANSWER: 1, AUTHORITY: 0, ADDITIONAL: 0
resolv  : ;; QUERY SECTION:
resolv  : ;;    www.google.com, type = A, class = IN
resolv  :
resolv  : ;; ANSWER SECTION:
resolv  : ;; www.google.com.        2m19s IN A  172.217.160.100
resolv  :
resolv  : Hex dump:
resolv  : 7483818000010001000000000377777706676f6f676c6503636f6d0000010001
resolv  : c00c000100010000008b0004acd9a064
```

5. Convert the logging into pbtext. Then, Clear the 'id' which is 0x7483 in
this example from 'query' and 'response' because 'id' is regenerated per
session. The follows is result pbtext.
```
config {
    call: CALL_GETHOSTBYNAME
    hostbyname {
        host: "www.google.com."
        family: GT_AF_INET
    };
}
result {
    return_code: GT_EAI_NO_ERROR
    addresses: "172.217.160.100"
}
packet_mapping {
    query:    "\x00\x00\x01\x00\x00\x01\x00\x00\x00\x00\x00\x00\x03\x77\x77\x77"
              "\x06\x67\x6f\x6f\x67\x6c\x65\x03\x63\x6f\x6d\x00\x00\x01\x00\x01"
    response: "\x00\x00\x81\x80\x00\x01\x00\x01\x00\x00\x00\x00\x03\x77\x77\x77"
              "\x06\x67\x6f\x6f\x67\x6c\x65\x03\x63\x6f\x6d\x00\x00\x01\x00\x01"
              "\xc0\x0c\x00\x01\x00\x01\x00\x00\x00\x8b\x00\x04\xac\xd9\xa0\x64"
}
```

## Decode packets in pbtext
You can invoke the [scapy](https://scapy.net) of python module to extract
binary packet record in pbtext file. Here are the instructions and example
for parsing packet.

### Instructions
Run the following instruction to decode.
```
$ python
>>> from scapy import all as scapy
>>> scapy.DNS("<paste_hex_string>").show2()
```

### Example
Using 'getaddrinfo.topsite.youtube.pbtxt' to be an example here.

1. Find the packet record 'query' or 'response' in .pbtext file.
```
query:    "\x00\x00\x01\x00\x00\x01\x00\x00\x00\x00\x00\x00\x03\x77\x77\x77"
          "\x07\x79\x6f\x75\x74\x75\x62\x65\x03\x63\x6f\x6d\x00\x00\x1c\x00"
          "\x01"
```
2. Run the following instruction.

Start python
```
$ python
```
Import scapy
```
>>> from scapy import all as scapy
```
Assign the binary packet to be decoded into a variable. Beware of using
backslash '\\' for new line if required
```
>>> raw_packet=\
    "\x00\x00\x01\x00\x00\x01\x00\x00\x00\x00\x00\x00\x03\x77\x77\x77" \
    "\x07\x79\x6f\x75\x74\x75\x62\x65\x03\x63\x6f\x6d\x00\x00\x1c\x00" \
    "\x01"
```
Decode packet
```
>>> scapy.DNS(raw_packet).show2()
###[ DNS ]###
  id        = 0
  qr        = 0
  opcode    = QUERY
  aa        = 0
  tc        = 0
  rd        = 1
  ra        = 0
  z         = 0
  ad        = 0
  cd        = 0
  rcode     = ok
  qdcount   = 1
  ancount   = 0
  nscount   = 0
  arcount   = 0
  \qd        \
   |###[ DNS Question Record ]###
   |  qname     = 'www.youtube.com.'
   |  qtype     = AAAA
   |  qclass    = IN
  an        = None
  ns        = None
  ar        = None
```

## Running the tests
Run the following instruction to test.
```
atest resolv_gold_test
```