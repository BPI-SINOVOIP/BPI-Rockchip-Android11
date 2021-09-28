# changelog

## 1.3.0

 - Added `encrypt_key_pref` (`1.2.840.113549.1.9.16.2.11`) to
   `cms.CMSAttributeType()`, along with related structures
 - Added Brainpool curves from RFC 5639 to `keys.NamedCurve()`
 - Fixed `x509.Certificate().subject_directory_attributes_value`
 - Fixed some incorrectly computed minimum elliptic curve primary key
   encoding sizes in `keys.NamedCurve()`
 - Fixed a `TypeError` when trying to call `.untag()` or `.copy()` on a
   `core.UTCTime()` or `core.GeneralizedTime()`, or a value containing one,
   when using Python 2

## 1.2.0

 - Added `asn1crypto.load_order()`, which returns a `list` of unicode strings
   of the names of the fully-qualified module names for all of submodules of
   the package. The module names are listed in their dependency load order.
   This is primarily intended for the sake of implementing hot reloading.

## 1.1.0

 - Added User ID (`0.9.2342.19200300.100.1.1`) to `x509.NameType()`
 - Added various EC named curves to `keys.NamedCurve()`

## 1.0.1

 - Fix an absolute import in `keys` to a relative import

## 1.0.0

 - Backwards Compatibility Breaks
    - `cms.KeyEncryptionAlgorithmId().native` now returns the value
      `"rsaes_pkcs1v15"` for OID `1.2.840.113549.1.1.1` instead of `"rsa"` 
    - Removed functionality to calculate public key values from private key
      values. Alternatives have been added to oscrypto.
       - `keys.PrivateKeyInfo().unwrap()` is now
         `oscrypto.asymmetric.PrivateKey().unwrap()`
       - `keys.PrivateKeyInfo().public_key` is now
         `oscrypto.asymmetric.PrivateKey().public_key.unwrap()`
       - `keys.PrivateKeyInfo().public_key_info` is now
         `oscrypto.asymmetric.PrivateKey().public_key.asn1`
       - `keys.PrivateKeyInfo().fingerprint` is now
         `oscrypto.asymmetric.PrivateKey().fingerprint`
       - `keys.PublicKeyInfo().unwrap()` is now
         `oscrypto.asymmetric.PublicKey().unwrap()`
       - `keys.PublicKeyInfo().fingerprint` is now
         `oscrypto.asymmetric.PublicKey().fingerprint`
 - Enhancements
    - Significantly improved parsing of `core.UTCTime()` and
      `core.GeneralizedTime()` values that include timezones and fractional
      seconds
    - `util.timezone` has a more complete implementation
    - `core.Choice()` may now be constructed by a 2-element tuple or a 1-key
      dict
    - Added `x509.Certificate().not_valid_before` and
      `x509.Certificate().not_valid_after`
    - Added `core.BitString().unused_bits`
    - Added `keys.NamedCurve.register()` for non-mainstream curve OIDs
    - No longer try to load optional performance dependency, `libcrypto`,
      on Mac or Linux
    - `ocsp.CertStatus().native` will now return meaningful unicode string
      values when the status choice is `"good"` or `"unknown"`. Previously
      both returned `None` due to the way the structure was designed.
    - Add support for explicit RSA SSA PSS (`1.2.840.113549.1.1.10`) to
      `keys.PublicKeyInfo()` and `keys.PrivateKeyInfo()`
    - Added structures for nested SHA-256 Windows PE signatures to
      `cms.CMSAttribute()`
    - Added RC4 (`1.2.840.113549.3.4`) to `algos.EncryptionAlgorithmId()`
    - Added secp256k1 (`1.3.132.0.10`) to `keys.NamedCurve()`
    - Added SHA-3 and SHAKE OIDs to `algos.DigestAlgorithmId()` and
      `algos.HmacAlgorithmId()`
    - Added RSA ES OAEP (`1.2.840.113549.1.1.7`) to
      `cms.KeyEncryptionAlgorithmId()`
    - Add IKE Intermediate (`1.3.6.1.5.5.8.2.2`) to `x509.KeyPurposeId()`
    - `x509.EmailAddress()` and `x509.DNSName()` now handle invalidly-encoded
      values using tags for `core.PrintableString()` and `core.UTF8String()`
    - Add parameter structue from RFC 5084 for AES-CCM to
      `algos.EncryptionAlgorithm()`
    - Improved robustness of parsing broken `core.Sequence()` and
      `core.SequenceOf()` values
 - Bug Fixes
    - Fixed encoding of tag values over 30
    - `core.IntegerBitString()` and `core.IntegerOctetString()` now restrict
      values to non-negative integers since negative values are not
      implemented
    - When copying or dumping a BER-encoded indefinite-length value,
      automatically force re-encoding to DER. *To ensure all nested values are
      always DER-encoded, `.dump(True)` must be called.*
    - Fix `UnboundLocalError` when calling `x509.IPAddress().native` on an
      encoded value that has a length of zero
    - Fixed passing `class_` via unicode string name to `core.Asn1Value()`
    - Fixed a bug where EC private keys with leading null bytes would be
      encoded in `keys.ECPrivateKey()` more narrowly than RFC 5915 requires
    - Fixed some edge-case bugs in `util.int_to_bytes()`
    - `x509.URI()` now only normalizes values when comparing
    - Fixed BER-decoding of indefinite length `core.BitString()`
    - Fixed DER-encoding of empty `core.BitString()`
    - Fixed a missing return value for `core.Choice().parse()`
    - Fixed `core.Choice().contents` working when the chosen alternative is a
      `core.Choice()` also
    - Fixed parsing and encoding of nested `core.Choice()` objects
    - Fixed a bug causing `core.ObjectIdentifier().native` to sometimes not
      map the OID
 - Packaging
    - `wheel`, `sdist` and `bdist_egg` releases now all include LICENSE,
      `sdist` includes docs
    - Added `asn1crypto_tests` package to PyPi

## 0.24.0

 - `x509.Certificate().self_signed` will no longer return `"yes"` under any
   circumstances. This helps prevent confusion since the library does not
   verify the signature. Instead a library like oscrypto should be used
   to confirm if a certificate is self-signed.
 - Added various OIDs to `x509.KeyPurposeId()`
 - Added `x509.Certificate().private_key_usage_period_value`
 - Added structures for parsing common subject directory attributes for
   X.509 certificates, including `x509.SubjectDirectoryAttribute()`
 - Added `algos.AnyAlgorithmIdentifier()` for situations where an
   algorithm identifier may contain a digest, signed digest or encryption
   algorithm OID
 - Fixed a bug with `x509.Certificate().subject_directory_attributes_value`
   not returning the correct value
 - Fixed a bug where explicitly-tagged fields in a `core.Sequence()` would
   not function properly when the field had a default value
 - Fixed a bug with type checking in `pem.armor()`

## 0.23.0

 - Backwards compatibility break: the `tag_type`, `explicit_tag` and
   `explicit_class` attributes on `core.Asn1Value` no longer exist and were
   replaced by the `implicit` and `explicit` attributes. Field param dicts
   may use the new `explicit` and `implicit` keys, or the old `tag_type` and
   `tag` keys. The attribute changes will likely to have little to no impact
   since they were primarily an implementation detail.
 - Teletex strings used inside of X.509 certificates are now interpreted
   using Windows-1252 (a superset of ISO-8859-1). This enables compatibility
   with certificates generated by OpenSSL. Strict parsing of Teletex strings
   can be retained by using the `x509.strict_teletex()` context manager.
 - Added support for nested explicit tagging, supporting values that are
   defined with explicit tagging and then added as a field of another
   structure using explicit tagging.
 - Fixed a `UnicodeDecodeError` when trying to find the (optional) dependency
   OpenSSL on Python 2
 - Fixed `next_update` field of `crl.TbsCertList` to be optional
 - Added the `x509.Certificate.sha256_fingerprint` property
 - `x509.Certificate.ocsp_urls` and `x509.DistributionPoint.url` will now
   return `https://`, `ldap://` and `ldaps://` URLs in addition to `http://`.
 - Added CMS Attribute Protection definitions from RFC 6211
 - Added OIDs from RFC 6962

## 0.22.0

 - Added `parser.peek()`
 - Implemented proper support for BER-encoded indefinite length strings of
   all kinds - `core.BitString`, `core.OctetString` and all of the `core`
   classes that are natively represented as Python unicode strings
 - Fixed a bug with encoding LDAP URLs in `x509.URI`
 - Correct `x509.DNSName` to allow a leading `.`, such as when used with
   `x509.NameConstraints`
 - Fixed an issue with dumping the parsed contents of `core.Any` when
   explicitly tagged
 - Custom `setup.py clean` now accepts the short `-a` flag for compatibility

## 0.21.1

 - Fixed a regression where explicit tagging of a field containing a
   `core.Choice` would result in an incorrect header
 - Fixed a bug where an `IndexError` was being raised instead of a `ValueError`
   when a value was truncated to not include enough bytes for the header
 - Corrected the spec for the `value` field of `pkcs12.Attribute`
 - Added support for `2.16.840.1.113894.746875.1.1` OID to
   `pkcs12.AttributeType`

## 0.21.0

 - Added `core.load()` for loading standard, universal types without knowing
   the spec beforehand
 - Added a `strict` keyword arg to the various `load()` methods and functions in
   `core` that checks for trailing data and raises a `ValueError` when found
 - Added `asn1crypto.parser` submodule with `emit()` and `parse()` functions for
   low-level integration
 - Added `asn1crypto.version` for version introspection without side-effects
 - Added `algos.DSASignature`
 - Fixed a bug with the `_header` attribute of explicitly-tagged values only
   containing the explicit tag header instead of both the explicit tag header
   and the encapsulated value header

## 0.20.0

 - Added support for year 0
 - Added the OID for unique identifier to `x509.NameType`
 - Fixed a bug creating the native representation of a `core.BitString` with
   leading null bytes
 - Added a `.cast()` method to allow converting between different
   representations of the same data, e.g. `core.BitString` and
   `core.OctetBitString`

## 0.19.0

 - Force `algos.DigestAlgorithm` to encoding `parameters` as `Null` when the
   `algorithm` is `sha1`, `sha224`, `sha256`, `sha384` or `sha512` per RFC 4055
 - Resolved an issue where a BER-encoded indefinite-length value could not be
   properly parsed when embedded inside of a `core.Sequence` or `core.Set`
 - Fix `x509.Name.build()` to properly handle dotted OID type values
 - `core.Choice` can now be constructed from a single-element `dict` or a
   two-element `tuple` to allow for better usability when constructing values
   from native Python values
 - All `core` objects can now be passed to `print()` with an exception being
   raised

## 0.18.5

 - Don't fail importing if `ctypes` or `_ctypes` is not available

## 0.18.4

 - `core.Sequence` will now raise an exception when an unknown field is provided
 - Prevent `UnicodeDecodeError` on Python 2 when calling
   `core.OctetString.debug()`
 - Corrected the default value for the `hash_algorithm` field of
   `tsp.ESSCertIDv2`
 - Fixed a bug constructing a `cms.SignedData` object
 - Ensure that specific RSA OIDs are always paired with `parameters` set to
   `core.Null`

## 0.18.3

 - Fixed DER encoding of `core.BitString` when a `_map` is specified (i.e. a
   "named bit list") to omit trailing zero bits. This fixes compliance of
   various `x509` structures with RFC 5280.
 - Corrected a side effect in `keys.PrivateKeyInfo.wrap()` that would cause the
   original `keys.ECPrivateKey` structure to become corrupt
 - `core.IntegerOctetString` now correctly encodes the integer as an unsigned
   value when converting to bytes. Previously decoding was unsigned, but
   encoding was signed.
 - Fix `util.int_from_bytes()` on Python 2 to return `0` from an empty byte
   string

## 0.18.2

 - Allow `_perf` submodule to be removed from source tree when embedding

## 0.18.1

 - Fixed DER encoding of `core.Set` and `core.SetOf`
 - Fixed a bug in `x509.Name.build()` that could generate invalid DER encoding
 - Improved exception messages when parsing nested structures via the `.native`
   attribute
 - `algos.SignedDigestAlgorithm` now ensures the `parameters` are set to
   `Null` when `algorithm` is `sha224_rsa`, `sha256_rsa`, `sha384_rsa` or
   `sha512_rsa`, per RFC 4055
 - Corrected the definition of `pdf.AdobeTimestamp` to mark the
   `requires_auth` field as optional
 - Add support for the OID `1.2.840.113549.1.9.16.2.14` to
   `cms.CMSAttributeType`
 - Improve attribute support for `cms.AttributeCertificateV2`
 - Handle `cms.AttributeCertificateV2` when incorrectly tagged as
   `cms.AttributeCertificateV1` in `cms.CertificateChoices`

## 0.18.0

 - Improved general parsing performance by 10-15%
 - Add support for Windows XP
 - Added `core.ObjectIdentifier.dotted` attribute to always return dotted
   integer unicode string
 - Added `core.ObjectIdentifier.map()` and `core.ObjectIdentifier.unmap()`
   class methods to map dotted integer unicode strings to user-friendly unicode
   strings and back
 - Added various Apple OIDs to `x509.KeyPurposeId`
 - Fixed a bug parsing nested indefinite-length-encoded values
 - Fixed a bug with `x509.Certificate.issuer_alt_name_value` if it is the first
   extension queried
 - `keys.PublicKeyInfo.bit_size` and `keys.PrivateKeyInfo.bit_size` values are
   now rounded up to the next closest multiple of 8

## 0.17.1

 - Fix a bug in `x509.URI` parsing IRIs containing explicit port numbers on
   Python 3.x

## 0.17.0

 - Added `x509.TrustedCertificate` for handling OpenSSL auxiliary certificate
   information appended after a certificate
 - Added `core.Concat` class for situations such as `x509.TrustedCertificate`
 - Allow "broken" X.509 certificates to use `core.IA5String` where an
   `x509.DirectoryString` should be used instead
 - Added `keys.PrivateKeyInfo.public_key_info` attribute
 - Added a bunch of OIDs to `x509.KeyPurposeId`

## 0.16.0

 - Added DH key exchange structures: `algos.KeyExchangeAlgorithm`,
   `algos.KeyExchangeAlgorithmId` and `algos.DHParameters`.
 - Added DH public key support to `keys.PublicKeyInfo`,
   `keys.PublicKeyAlgorithm` and `keys.PublicKeyAlgorithmId`. New structures
   include `keys.DomainParameters` and `keys.ValidationParms`.

## 0.15.1

 - Fixed `cms.CMSAttributes` to be a `core.SetOf` instead of `core.SequenceOf`
 - `cms.CMSAttribute` can now parse unknown attribute contrustruct without an
   exception being raised
 - `x509.PolicyMapping` now uses `x509.PolicyIdentifier` for field types
 - Fixed `pdf.RevocationInfoArchival` so that all fields are now of the type
   `core.SequenceOf` instead of a single value
 - Added support for the `name_distinguisher`, `telephone_number` and
   `organization_identifier` OIDs to `x509.Name`
 - Fixed `x509.Name.native` to not accidentally create nested lists when three
   of more values for a single type are part of the name
 - `x509.Name.human_friendly` now reverses the order of fields when the data
   in an `x509.Name` was encoded in most-specific to least-specific order, which
   is the opposite of the standard way of least-specific to most-specific.
 - `x509.NameType.human_friendly` no longer raises an exception when an
   unknown OID is encountered
 - Raise a `ValueError` when parsing a `core.Set` and an unknown field is
   encountered

## 0.15.0

 - Added support for the TLS feature extension from RFC 7633
 - `x509.Name.build()` now accepts a keyword parameter `use_printable` to force
   string encoding to be `core.PrintableString` instead of `core.UTF8String`
 - Added the functions `util.uri_to_iri()` and `util.iri_to_uri()`
 - Changed `algos.SignedDigestAlgorithmId` to use the preferred OIDs when
   mapping a unicode string name to an OID. Previously there were multiple OIDs
   for some algorithms, and different OIDs would sometimes be selected due to
   the fact that the `_map` `dict` is not ordered.

## 0.14.1

 - Fixed a bug generating `x509.Certificate.sha1_fingerprint` on Python 2

## 0.14.0

 - Added the `x509.Certificate.sha1_fingerprint` attribute

## 0.13.0

 - Backwards compatibility break: the native representation of some
   `algos.EncryptionAlgorithmId` values changed. `aes128` became `aes128_cbc`,
   `aes192` became `aes192_cbc` and `aes256` became `aes256_cbc`.
 - Added more OIDs to `algos.EncryptionAlgorithmId`
 - Added more OIDs to `cms.KeyEncryptionAlgorithmId`
 - `x509.Name.human_friendly` now properly supports multiple values per
   `x509.NameTypeAndValue` object
 - Added `ocsp.OCSPResponse.basic_ocsp_response` and
   `ocsp.OCSPResponse.response_data` properties
 - Added `algos.EncryptionAlgorithm.encryption_mode` property
 - Fixed a bug with parsing times containing timezone offsets in Python 3
 - The `attributes` field of `csr.CertificationRequestInfo` is now optional,
   for compatibility with other ASN.1 parsers

## 0.12.2

 - Correct `core.Sequence.__setitem__()` so set `core.VOID` to an optional
   field when `None` is set

## 0.12.1

 - Fixed a `unicode`/`bytes` bug with `x509.URI.dump()` on Python 2

## 0.12.0

 - Backwards Compatibility Break: `core.NoValue` was renamed to `core.Void` and
   a singleton was added as `core.VOID`
 - 20-30% improvement in parsing performance
 - `core.Void` now implements `__nonzero__`
 - `core.Asn1Value.copy()` now performs a deep copy
 - All `core` value classes are now compatible with the `copy` module
 - `core.SequenceOf` and `core.SetOf` now implement `__contains__`
 - Added `x509.Name.__len__()`
 - Fixed a bug where `core.Choice.validate()` would not properly account for
   explicit tagging
 - `core.Choice.load()` now properly passes itself as the spec when parsing
 - `x509.Certificate.crl_distribution_points` no longer throws an exception if
   the `DistributionPoint` does not have a value for the `distribution_point`
   field

## 0.11.1

 - Corrected `core.UTCTime` to interpret year <= 49 as 20xx and >= 50 as 19xx
 - `keys.PublicKeyInfo.hash_algo` can now handle DSA keys without parameters
 - Added `crl.CertificateList.sha256` and `crl.CertificateList.sha1`
 - Fixed `x509.Name.build()` to properly encode `country_name`, `serial_number`
   and `dn_qualifier` as `core.PrintableString` as specified in RFC 5280,
   instead of `core.UTF8String`

## 0.11.0

 - Added Python 2.6 support
 - Added ability to compare primitive type objects
 - Implemented proper support for internationalized domains, URLs and email
   addresses in `x509.Certificate`
 - Comparing `x509.Name` and `x509.GeneralName` objects adheres to RFC 5280
 - `x509.Certificate.self_signed` and `x509.Certificate.self_issued` no longer
   require that certificate is for a CA
 - Fixed `x509.Certificate.valid_domains` to adhere to RFC 6125
 - Added `x509.Certificate.is_valid_domain_ip()`
 - Added `x509.Certificate.sha1` and `x509.Certificate.sha256`
 - Exposed `util.inet_ntop()` and `util.inet_pton()` for IP address encoding
 - Improved exception messages for improper types to include type's module name

## 0.10.1

 - Fixed bug in `core.Sequence` affecting Python 2.7 and pypy

## 0.10.0

 - Added PEM encoding/decoding functionality
 - `core.BitString` now uses item access instead of attributes for named bit
   access
 - `core.BitString.native` now uses a `set` of unicode strings when `_map` is
   present
 - Removed `core.Asn1Value.pprint()` method
 - Added `core.ParsableOctetString` class
 - Added `core.ParsableOctetBitString` class
 - Added `core.Asn1Value.copy()` method
 - Added `core.Asn1Value.debug()` method
 - Added `core.SequenceOf.append()` method
 - Added `core.Sequence.spec()` and `core.SequenceOf.spec()` methods
 - Added correct IP address parsing to `x509.GeneralName`
 - `x509.Name` and `x509.GeneralName` are now compared according to rules in
   RFC 5280
 - Added convenience attributes to:
   - `algos.SignedDigestAlgorithm`
   - `crl.CertificateList`
   - `crl.RevokedCertificate`
   - `keys.PublicKeyInfo`
   - `ocsp.OCSPRequest`
   - `ocsp.Request`
   - `ocsp.OCSPResponse`
   - `ocsp.SingleResponse`
   - `x509.Certificate`
   - `x509.Name`
 - Added `asn1crypto.util` module with the following items:
   - `int_to_bytes()`
   - `int_from_bytes()`
   - `timezone.utc`
 - Added `setup.py clean` command

## 0.9.0

 - Initial release
