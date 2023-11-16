About Crypto Engine sample code
===============================

af_alg:
        Sample code which using socket's AF_ALG API to process data encryption and decryption.
        AF_ALG API is the user space interface to kernel's crypto subsystem.

kcapi:
        Sample code which using libkcapi's API to process data encryption and decryption.
        libkcapi is a library which base on AF_ALG but providing more convenience interface for user.

openssl:
        Sample code which using OpenSSL's API to process data encryption and decryption.
        OpenSSL hardware engine for AIC SOC is base on libkcapi.
