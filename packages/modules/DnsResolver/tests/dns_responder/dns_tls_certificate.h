/*
 * Copyright (C) 2019 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

#pragma once

// Currently the hostname of TLS server must match the CN filed on the server's certificate.
// Inject a test CA whose hostname is "example.com" for DNS-OVER-TLS tests.
static constexpr char kDefaultPrivateDnsHostName[] = "example.com";
static constexpr char kDefaultIncorrectPrivateDnsHostName[] = "www.example.com";

/*
 * test cert, key, and rootca files can be generated using openssl with
 * the following steps:
 *
 * 1. Create CA certificate:
 * $ openssl genrsa 2048 > ca_key.pem
 * $ openssl req -new -sha256 -x509 -nodes -days 3650 -key ca_key.pem -out ca_certificate.pem -subj
 * '/C=/ST=/L=/CN=/emailAddress='
 *
 * 2. Create private key:
 * $ openssl req -sha256 -newkey rsa:2048 -days 3650 -nodes -keyout server_key.pem -out
 * server_req.pem -subj '/C=/ST=/L=/CN=example.com/emailAddress='
 * $ openssl rsa -in server_key.pem -out server_key.pem
 *
 * 3. Create server certificate:
 * $ openssl x509 -sha256 -req -in server_req.pem -days 3650 -CA
 * ca_certificate.pem -CAkey ca_key.pem -set_serial 01 -out server_certificate.pem
 *
 * 4. Verify the certificate:
 * $ openssl verify -CAfile ca_certificate.pem server_certificate.pem
 */
// ca_certificate.pem
static constexpr char kCaCert[] = R"(
-----BEGIN CERTIFICATE-----
MIIC4TCCAcmgAwIBAgIUQUHZnWhL6M4qcS+I0lLkMyqf3VMwDQYJKoZIhvcNAQEL
BQAwADAeFw0xOTA2MTAwODM3MzlaFw0yOTA2MDcwODM3MzlaMAAwggEiMA0GCSqG
SIb3DQEBAQUAA4IBDwAwggEKAoIBAQCapRbBg6dRT4id4DxmlyktomE8gpm4W+VA
ZOivhKat4CvGfVjVIAUYxV7LOGREkkT8Qhn5/gU0lShsnURzEDWY+IjMDDw+kRAm
iFAlMRnCobTp/tenseNRB2tDuUhkRbzaT6qaidPbKy099p909gxf4YqsgY2NfsY2
JkideqIkVq2hvLehsu3BgiK06TGUgxOHfj74vx7iGyujq1v38J1hlox5vj/svJF6
jVdDw8p2UkJbO2N9u3al0nNSMG+MCgd3cvKUySTnjedYXsYB0WyH/JZn//KDq6o+
as6eQVHuH1fu+8XPzBNATlkHzy+YAs7T+UWbkoa1F8wIElVQg66lAgMBAAGjUzBR
MB0GA1UdDgQWBBShu/e54D3VdqdLOWo9Ou5hbjaIojAfBgNVHSMEGDAWgBShu/e5
4D3VdqdLOWo9Ou5hbjaIojAPBgNVHRMBAf8EBTADAQH/MA0GCSqGSIb3DQEBCwUA
A4IBAQBFkEGqqzzdQlhP5m1kzh+SiUCwekzSump0CSk0JAXAdeLNbWs3H+pE1/hM
Fx7oFonoX5O6pi68JfcIP0u7wNuZkKqubUl4gG6aHDkAad2oeTov0Be7EKt8Ekwf
tmFWVQQrF9otlG3Stn4vmE5zVNGQXDgRsNKPekSo0XJz757q5WgblauB71Rekvio
TCUXXt3jf3SuovcUFjgBkaohikBRbLiPWZrW4y0XUsgBKI6sLtiSZOPiNevY2xAR
y7mCSmi4wP7vtUQ5G8znkAMKoo0FzyfjSogGQeREUM8Oe9Mmh/D39sq/D4TsiAxt
Pwl59DlzlHHJhmOL+SCGciBX4X7p
-----END CERTIFICATE-----
)";

// server_key.pem
static constexpr char kPrivatekey[] = R"(
-----BEGIN RSA PRIVATE KEY-----
MIIEowIBAAKCAQEAuo/v4VuY0Ees5HRx+NwTGm/bgToUFjq9R4z4FX+j8yyohxS8
OxQZzpKu8JJytyPPi+SnXqZB25usGBPJHapD1Q5YYCIZF9EBztIqnEDbxvcWBrv7
NDDhPMQ6v5YFhAIUN3a1yBESBWQOWsNkwJw04Wc4agZrhhnG/vS7gu1gn+CnaDYu
pAmHrGS9cSV/B9ZCpLhis2JxmULgdz6ZBee/x8dHHFd1Qeb/+G8jhBqhYbQK7ZFL
mIO3DXrlP/ONXJ8IE2+PPDloiotkY5ar/1ZbRQS9fSKM9J6pipOEbAI1QF+tEn1b
naLfJfoMHIcb0p5xr04OALUZOGw4iVfxulMRIQIDAQABAoIBACDLLF9wumviLYH6
9g3IoZMEFpGgo+dEbAEnxnQA+9DDCNy1yGCaJ+8n2ZhwJboLkXAFwWXh07HGq3mQ
AMo2I7ZPzzkWxVJqaubwCo1s2TUgOb71TDLgZLdJxwnmVRHfS650L3/7gC9yZxON
RSiWTLVSb5gziLMJ1PD8E/nvwAxaJDlT6vzqwRbnHBkQoumTmds2ecLJd2/6pfl4
bMhtIKA3ULqnJlqlRt6ds/pWU9ttmXEX52uaGhzaF7PRomOW5pKR6CyBzNCn/RNF
ZPIINW1TVWss9NMZsJLdIzs7Oon5gQYil9rU2uiA5ZUanYDIL9DOMrfAM3hfUuFq
ZOhfBAECgYEA36CT81EkdDE7pum/kIuCG3wDEls+xNbWmF76IJAxnolJzKvJsdJA
0za/l1Qe3/bRYHZWKc7by45LFYefOsS29jqBnBBMLurI7OLtcXqkTSSm11AfsDDI
gw4bKs81TYdHhnbIDGeApfSWOGXgDM/j4N3stuvY1lOIocXqKMomZVMCgYEA1ZHD
jtxeAmCqzJHIJ4DOY0Y2RR3Bq3ue/mc8gmV9wDyJMMvBpvOoRkUjwbKZToi4Px30
5fn6SCRtOKfEL0b9LV7JFsMr84Zoj8kjtnE0BdNfQqdE/uWltpATl6YUPlzqZTGs
HBGVpsNCzYkjFu9m/zIiryCHY8Uut3VEZmNJjTsCgYEAgADBTzAuBpg7xeHcdhd0
xNiqRXKXLkKvGQ6ca9E9pbp91LqsO63Wz09yQWO0PIxh8q4pycqPQyfS0KMNwKzi
8XQxxiwJ/30Cv51xPlht/X4yReKmEMsLqwCDCnEK2LLLfSs2fOst11B2QBgINC03
CfrdySKcvqmX9sl7rBdx/OMCgYB9t2o4RDwKhkDEXuRFbKsRARmdIeEJQqHa+4ZA
8+FMMdZIJQj/b9qUUsqzkKBx/EUI0meAoN/Va6vnd8oiUlViSbNxdL4AghQ2353o
HUcUTtJ6d+BDc4dSqgj+ccLk2ukXXGAFvcwr+DDwsFM5gv9MJYUJNcq8ziurzpnO
848uVQKBgEmyAa2jt1qNpAvxU0MakJIuKhQl2b6/54EKi9WKqIMs1+rKk6O/Ck3n
+tEWqHhZ4uCRmvTgpOM821l4fTHsoJ8IGWV0mwfk95pEL+g/eBLExR4etMqaW9uz
x8vnVTKNzZsAVgRcemcLqzuyuMg+/ZnH+YNMzMl0Nbkt+kE3FhfM
-----END RSA PRIVATE KEY-----
)";

// server_certificate.pem
static constexpr char kCertificate[] = R"(
-----BEGIN CERTIFICATE-----
MIICijCCAXICAQEwDQYJKoZIhvcNAQELBQAwADAeFw0xOTA2MTAwODM3MzlaFw0y
OTA2MDcwODM3MzlaMBYxFDASBgNVBAMMC2V4YW1wbGUuY29tMIIBIjANBgkqhkiG
9w0BAQEFAAOCAQ8AMIIBCgKCAQEAuo/v4VuY0Ees5HRx+NwTGm/bgToUFjq9R4z4
FX+j8yyohxS8OxQZzpKu8JJytyPPi+SnXqZB25usGBPJHapD1Q5YYCIZF9EBztIq
nEDbxvcWBrv7NDDhPMQ6v5YFhAIUN3a1yBESBWQOWsNkwJw04Wc4agZrhhnG/vS7
gu1gn+CnaDYupAmHrGS9cSV/B9ZCpLhis2JxmULgdz6ZBee/x8dHHFd1Qeb/+G8j
hBqhYbQK7ZFLmIO3DXrlP/ONXJ8IE2+PPDloiotkY5ar/1ZbRQS9fSKM9J6pipOE
bAI1QF+tEn1bnaLfJfoMHIcb0p5xr04OALUZOGw4iVfxulMRIQIDAQABMA0GCSqG
SIb3DQEBCwUAA4IBAQAuI2NjdWiD2lwmRraW6C7VBF+Sf+9QlzTVzSjuDbPkkYIo
YWpeYsEeFO5NlxxXl77iu4MqznSAOK8GCiNDCCulDNWRhd5lcO1dVHLcIFYKZ+xv
6IuH3vh60qJ2hoZbalwflnMQklqh3745ZyOH79dzKTFvlWyNJ2hQgP9bZ2g8F4od
dS7aOwvx3DCv46b7vBJMKd53ZCdHubfFebDcGxc60DUR0fLSI/o1MJgriODZ1SX7
sxwzrxvbJW0T+gJOL0C0lE6D84F9oL2u3ef17fC5u1bRd/77pzjTM+dQe7sZspCz
iboTujdUqo+NSdWgwPUTGTYQg/1i9Qe0vjc0YplY
-----END CERTIFICATE-----
)";