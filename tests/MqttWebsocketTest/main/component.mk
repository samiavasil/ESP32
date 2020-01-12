#
# "main" pseudo-component makefile.
#
# (Uses default behaviour of compiling all source files in directory, adding 'include' to include path.)

# embed files from the "certs" directory as binary data symbols
# in the app
#COMPONENT_PRIV_INCLUDEDIRS +=  ota/certs
#COMPONENT_EMBED_TXTFILES := wpa2_ca.pem
#COMPONENT_EMBED_TXTFILES += mqtt_eclipse_org.pem
#COMPONENT_EMBED_TXTFILES += wpa2_client.crt
#COMPONENT_EMBED_TXTFILES += wpa2_client.key
#COMPONENT_EMBED_TXTFILES += test_cert.pem
