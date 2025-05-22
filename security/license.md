# License Setup
### Get the Raspberry Pi's serial number (best unique ID)
```
cat /proc/cpuinfo | grep Serial | cut -d ' ' -f 2
```
### Generate private key
```
openssl genpkey -algorithm RSA -out private_key.pem -pkeyopt rsa_keygen_bits:2048
```

### Extract public key from private key
```
openssl rsa -pubout -in private_key.pem -out public_key.pem
```

### Create license content with CPU ID and your secret
```
echo "cpu_id=68dc22faefaf1e1e;secret=lscm" > license_content.txt
```

### Sign the license content
```
openssl dgst -sha256 -sign private_key.pem -out signature.bin license_content.txt
```

### Structure the license.dat
```
cat license_content.txt > license.dat
echo "---SIGNATURE---" >> license.dat
base64 signature.bin >> license.dat
```

### Move the public key and license.dat to pi
### Run the bash script to move to a secure directory with priviledges
```
sudo bash.sh
```
Resulting:
- /opt/Drip/license.dat
- /opt/Drip/public_key.pem

