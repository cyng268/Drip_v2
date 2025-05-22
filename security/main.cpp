#include <iostream>
#include <fstream>
#include <string>
#include <cstdio>
#include <vector>
#include <openssl/rsa.h>
#include <openssl/pem.h>
#include <openssl/err.h>
#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/buffer.h>

// Base64 decode function to avoid using system commands
std::vector<unsigned char> base64Decode(const std::string& input) {
    BIO* b64 = BIO_new(BIO_f_base64());
    BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
    
    BIO* bmem = BIO_new_mem_buf(input.c_str(), input.length());
    bmem = BIO_push(b64, bmem);
    
    std::vector<unsigned char> result(input.length());
    int decodedSize = BIO_read(bmem, result.data(), input.length());
    result.resize(decodedSize > 0 ? decodedSize : 0);
    
    BIO_free_all(bmem);
    return result;
}

// Read current device's CPU ID
std::string getCPUID() {
    FILE* fp = popen("cat /proc/cpuinfo | grep Serial | cut -d ' ' -f 2", "r");
    char buffer[256];
    std::string result = "";
    if (fp) {
        while (!feof(fp)) {
            if (fgets(buffer, sizeof(buffer), fp) != NULL) {
                result += buffer;
            }
        }
        pclose(fp);
    }
    // Trim whitespace
    result.erase(result.find_last_not_of("\n\r\t ") + 1);
    return result;
}

bool verifyLicense() {
    // Read license file
    std::ifstream licenseFile("/opt/Drip/license.dat");
    if (!licenseFile.is_open()) {
        std::cerr << "License file not found!" << std::endl;
        return false;
    }
    
    std::string licenseContent;
    std::string line;
    bool readingSignature = false;
    std::string signature;
    
    while (std::getline(licenseFile, line)) {
        if (line == "---SIGNATURE---") {
            readingSignature = true;
            continue;
        }
        
        if (readingSignature) {
            signature += line;
        } else {
            licenseContent += line + "\n";
        }
    }
    licenseFile.close();
    
    // Extract CPU ID from license
    size_t cpuIdStart = licenseContent.find("cpu_id=") + 7;
    size_t cpuIdEnd = licenseContent.find(";", cpuIdStart);
    std::string licenseCpuId = licenseContent.substr(cpuIdStart, cpuIdEnd - cpuIdStart);
    
    // Check if CPU ID matches current device
    std::string currentCpuId = getCPUID();
    if (licenseCpuId != currentCpuId) {
        std::cerr << "License not valid for this device!" << std::endl;
        return false;
    }
    
    // Verify signature
    FILE* pubKeyFile = fopen("/opt/Drip/public_key.pem", "r");
    if (!pubKeyFile) {
        std::cerr << "Public key file not found!" << std::endl;
        return false;
    }
    
    EVP_PKEY* pubKey = PEM_read_PUBKEY(pubKeyFile, NULL, NULL, NULL);
    fclose(pubKeyFile);
    
    if (!pubKey) {
        std::cerr << "Failed to load public key!" << std::endl;
        return false;
    }
    
    // Decode base64 signature
    std::vector<unsigned char> decodedSignature = base64Decode(signature);
    if (decodedSignature.empty()) {
        std::cerr << "Failed to decode signature!" << std::endl;
        EVP_PKEY_free(pubKey);
        return false;
    }
    
    // Create the verification context
    EVP_MD_CTX* mdctx = EVP_MD_CTX_new();
    if (!mdctx) {
        std::cerr << "Failed to create verification context!" << std::endl;
        EVP_PKEY_free(pubKey);
        return false;
    }
    
    // Initialize verification
    if (EVP_DigestVerifyInit(mdctx, NULL, EVP_sha256(), NULL, pubKey) != 1) {
        std::cerr << "Failed to initialize verification!" << std::endl;
        EVP_MD_CTX_free(mdctx);
        EVP_PKEY_free(pubKey);
        return false;
    }
    
    // Add data to be verified
    if (EVP_DigestVerifyUpdate(mdctx, licenseContent.c_str(), licenseContent.length()) != 1) {
        std::cerr << "Failed to update verification!" << std::endl;
        EVP_MD_CTX_free(mdctx);
        EVP_PKEY_free(pubKey);
        return false;
    }
    
    // Verify the signature
    int verifyResult = EVP_DigestVerifyFinal(mdctx, decodedSignature.data(), decodedSignature.size());
    
    // Clean up
    EVP_MD_CTX_free(mdctx);
    EVP_PKEY_free(pubKey);
    
    if (verifyResult != 1) {
        std::cerr << "Signature verification failed!" << std::endl;
        return false;
    }
    
    return true;
}

int main() {
    if (!verifyLicense()) {
        std::cerr << "License verification failed. Exiting." << std::endl;
        return 1;
    }
    
    std::cout << "License verified successfully. Running application..." << std::endl;
    
    // Your application code goes here
    
    return 0;
}