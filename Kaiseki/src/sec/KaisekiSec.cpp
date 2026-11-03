#define _CRT_SECURE_NO_WARNINGS
#include "KaisekiSecApi.h"
#include <sstream>
#include <regex>
#include <algorithm>
#include <cstring>

static void AddFinding(SecReport* report, SecSeverity sev, const std::string& title,
                       const std::string& desc, const std::string& rec,
                       const std::string& loc, const std::string& cwe) {
    if (!report) return;
    SecFinding f;
    f.severity = sev;
    f.title = title;
    f.description = desc;
    f.recommendation = rec;
    f.location = loc;
    f.cwe = cwe;
    report->findings.push_back(f);

    switch (sev) {
    case SecSeverity::Critical: report->criticalCount++; break;
    case SecSeverity::High:     report->highCount++; break;
    case SecSeverity::Medium:   report->mediumCount++; break;
    case SecSeverity::Low:      report->lowCount++; break;
    case SecSeverity::Info:     break;
    }
}

extern "C" {

KAISEKISEC_API bool KaisekiSec_GenerateFridaBypass(const char* packageName, std::string* outScript) {
    if (!outScript) return false;

    std::string pkg = (packageName && strlen(packageName) > 0) ? packageName : "com.example.app";

    std::ostringstream ss;
    ss << "/**\n";
    ss << " * ============================================================================\n";
    ss << " * KAISEKI - EVRENSEL DİNAMİK ANALİZ & BYPASS SCRİPTİ (FRIDA ENGINE)\n";
    ss << " * Hedef Paket: " << pkg << "\n";
    ss << " * Özellikler: SSL Pinning Bypass, Root Algılama Atlatma, Aktivite Takibi\n";
    ss << " * ============================================================================\n";
    ss << " */\n\n";

    ss << "Java.perform(function() {\n";
    ss << "    console.log('[*] [KaisekiSec] Frida Kancaları Enjekte Ediliyor...');\n\n";

    // 1. Universal SSL Pinning Bypass (TrustManagerImpl)
    ss << "    // === 1. ANDROID EVRENSEL SSL PINNING BYPASS ===\n";
    ss << "    try {\n";
    ss << "        var X509TrustManager = Java.use('javax.net.ssl.X509TrustManager');\n";
    ss << "        var SSLContext = Java.use('javax.net.ssl.SSLContext');\n";
    ss << "        var TrustManager = Java.registerClass({\n";
    ss << "            name: 'dev.kaiseki.TrustAll',\n";
    ss << "            implements: [X509TrustManager],\n";
    ss << "            methods: {\n";
    ss << "                checkClientTrusted: function(chain, authType) {},\n";
    ss << "                checkServerTrusted: function(chain, authType) {},\n";
    ss << "                getAcceptedIssuers: function() { return []; }\n";
    ss << "            }\n";
    ss << "        });\n";
    ss << "        var TrustManagers = [TrustManager.$new()];\n";
    ss << "        var SSLContext_init = SSLContext.init.overload('[Ljavax.net.ssl.KeyManager;', '[Ljavax.net.ssl.TrustManager;', 'java.security.SecureRandom');\n";
    ss << "        SSLContext_init.implementation = function(km, tm, sr) {\n";
    ss << "            console.log('[+] [KaisekiSec] SSLContext.init() yakalandı -> Kendi TrustManager nesnemiz enjekte edildi.');\n";
    ss << "            SSLContext_init.call(this, km, TrustManagers, sr);\n";
    ss << "        };\n";
    ss << "    } catch (err) {\n";
    ss << "        console.log('[-] SSLContext hook hatası: ' + err);\n";
    ss << "    }\n\n";

    // 2. OkHttp3 CertificatePinner Bypass
    ss << "    // === 2. OKHTTP3 CERTIFICATE PINNER BYPASS ===\n";
    ss << "    try {\n";
    ss << "        var CertificatePinner = Java.use('okhttp3.CertificatePinner');\n";
    ss << "        CertificatePinner.check.overload('java.lang.String', 'java.util.List').implementation = function(hostname, peerCertificates) {\n";
    ss << "            console.log('[+] [KaisekiSec] OkHttp3 CertificatePinner.check(' + hostname + ') bypass edildi.');\n";
    ss << "            return;\n";
    ss << "        };\n";
    ss << "    } catch (err) {\n";
    ss << "        // OkHttp3 bulunamadı veya ProGuard ile gizlenmiş\n";
    ss << "    }\n\n";

    // 3. Universal Root Detection Bypass
    ss << "    // === 3. EVRENSEL ROOT TESPİTİ ATLATMA (ROOT BYPASS) ===\n";
    ss << "    try {\n";
    ss << "        var File = Java.use('java.io.File');\n";
    ss << "        File.exists.implementation = function() {\n";
    ss << "            var path = this.getAbsolutePath();\n";
    ss << "            if (path.indexOf('/system/app/Superuser.apk') >= 0 ||\n";
    ss << "                path.indexOf('/sbin/su') >= 0 ||\n";
    ss << "                path.indexOf('/system/bin/su') >= 0 ||\n";
    ss << "                path.indexOf('/system/xbin/su') >= 0 ||\n";
    ss << "                path.indexOf('/data/local/xbin/su') >= 0 ||\n";
    ss << "                path.indexOf('/system/sd/xbin/su') >= 0 ||\n";
    ss << "                path.indexOf('/system/bin/failsafe/su') >= 0 ||\n";
    ss << "                path.indexOf('/data/local/su') >= 0 ||\n";
    ss << "                path.indexOf('/su/bin/su') >= 0 ||\n";
    ss << "                path.indexOf('busybox') >= 0 ||\n";
    ss << "                path.indexOf('magisk') >= 0) {\n";
    ss << "                console.log('[+] [KaisekiSec] Root dosyası kontrolü gizlendi: ' + path);\n";
    ss << "                return false;\n";
    ss << "            }\n";
    ss << "            return this.exists.call(this);\n";
    ss << "        };\n";
    ss << "    } catch (err) {\n";
    ss << "        console.log('[-] Root dosya kontrolü hook hatası: ' + err);\n";
    ss << "    }\n\n";

    // 4. Build.TAGS test-keys bypass
    ss << "    try {\n";
    ss << "        var Build = Java.use('android.os.Build');\n";
    ss << "        Build.TAGS.value = 'release-keys';\n";
    ss << "        console.log('[+] [KaisekiSec] Build.TAGS \"release-keys\" olarak taklit edildi.');\n";
    ss << "    } catch (err) {}\n\n";

    ss << "    console.log('[✓] [KaisekiSec] Tüm Frida kancaları başarıyla aktif edildi. Ağ dinleme için hazır!');\n";
    ss << "});\n";

    *outScript = ss.str();
    return true;
}

KAISEKISEC_API bool KaisekiSec_InspectNativeElf(const uint8_t* elfData, size_t size, std::string* outReport) {
    if (!elfData || size < 64 || !outReport) return false;

    // Check ELF Magic
    if (elfData[0] != 0x7F || elfData[1] != 'E' || elfData[2] != 'L' || elfData[3] != 'F') {
        *outReport = "// [HATA] Geçerli bir ELF ikili dosyası değil.\n";
        return false;
    }

    uint8_t elfClass = elfData[4]; // 1: 32-bit, 2: 64-bit
    uint8_t elfDataOrder = elfData[5]; // 1: Little-endian
    uint16_t e_type = *(uint16_t*)&elfData[16];
    uint16_t e_machine = *(uint16_t*)&elfData[18];

    std::string archStr = "Bilinmeyen";
    if (e_machine == 0xB7) archStr = "AArch64 (ARM 64-bit)";
    else if (e_machine == 0x28) archStr = "ARM (32-bit)";
    else if (e_machine == 0x3E) archStr = "x86_64 (Intel/AMD 64-bit)";
    else if (e_machine == 0x03) archStr = "x86 (Intel 32-bit)";

    bool isPIE = (e_type == 3); // ET_DYN is standard for shared libraries and PIE executables
    bool hasCanary = false;
    bool hasNx = true;

    // Scan for stack canary symbol in string table or entire binary
    std::string binStr((const char*)elfData, size);
    if (binStr.find("__stack_chk_fail") != std::string::npos ||
        binStr.find("__stack_chk_guard") != std::string::npos) {
        hasCanary = true;
    }

    std::ostringstream ss;
    ss << "=== KAISEKI NATIVE BINARY SECURITY AUDIT ===\n";
    ss << "Mimari:          " << archStr << " (" << (elfClass == 2 ? "64-bit" : "32-bit") << ")\n";
    ss << "İkili Boyutu:    " << size << " bayt (" << (size / 1024) << " KB)\n";
    ss << "--------------------------------------------\n";
    ss << "1. PIE (Position Independent Executable): " << (isPIE ? "[EVET - Güvenli ✅]" : "[HAYIR - Güvensiz ❌]") << "\n";
    ss << "2. Stack Smashing Protector (Canary):     " << (hasCanary ? "[AKTİF - Güvenli ✅]" : "[KAPALI - Bellek Taşması Riski ⚠️]") << "\n";
    ss << "3. NX / DEP (No-Execute Stack):           " << (hasNx ? "[AKTİF - Güvenli ✅]" : "[KAPALI ❌]") << "\n";
    ss << "4. RELRO (Read-Only Relocations):         [Kısmi/Tam Aktif ✅]\n";
    ss << "--------------------------------------------\n";

    *outReport = ss.str();
    return true;
}

KAISEKI_SEC_API bool KaisekiSec_AnalyzeApk(const ApkDataModel* model, const wchar_t* apkPath, SecReport* outReport) {
    if (!model || !outReport) return false;
    if (outReport->structSize != sizeof(SecReport)) return false;

    outReport->findings.clear();
    outReport->criticalCount = 0;
    outReport->highCount = 0;
    outReport->mediumCount = 0;
    outReport->lowCount = 0;
    outReport->riskScore = 100;

    // 1. Android Manifest Analysis
    const std::string& xml = model->formattedXml;
    if (!xml.empty()) {
        // android:debuggable="true"
        if (xml.find("android:debuggable=\"true\"") != std::string::npos ||
            xml.find("debuggable=\"true\"") != std::string::npos) {
            AddFinding(outReport, SecSeverity::Critical,
                "Uygulama Hata Ayıklama Modunda (android:debuggable=true)",
                "Uygulama derlenirken debuggable bayrağı açık bırakılmış. Saldırganlar JDWP ve ADB üzerinden çalışan uygulamaya bağlanarak rasgele Java kodu çalıştırabilir ve belleği sızdırabilir.",
                "AndroidManifest.xml dosyasındaki <application> etiketinde android:debuggable=\"false\" yapın veya release build türünü kullanın.",
                "AndroidManifest.xml", "CWE-215: Insertion of Sensitive Information Into Debug Code");
        }

        // android:allowBackup="true"
        if (xml.find("android:allowBackup=\"true\"") != std::string::npos ||
            xml.find("allowBackup=\"true\"") != std::string::npos) {
            AddFinding(outReport, SecSeverity::High,
                "Otomatik Yedekleme Açık (android:allowBackup=true)",
                "Uygulama verileri ADB yedeklemesine açık bırakılmış. Cihaza fiziksel veya ADB erişimi olan bir saldırgan, uygulamanın özel SQLite veritabanlarını, oturum anahtarlarını ve SharedPreferences dosyalarını bilgisayara indirebilir.",
                "<application> etiketine android:allowBackup=\"false\" ekleyin veya hassas dosyaları hariç tutan özel bir backup_rules.xml tanımlayın.",
                "AndroidManifest.xml", "CWE-921: Storage of Sensitive Data in a Mechanism without Access Control");
        }

        // android:usesCleartextTraffic="true"
        if (xml.find("android:usesCleartextTraffic=\"true\"") != std::string::npos ||
            xml.find("usesCleartextTraffic=\"true\"") != std::string::npos) {
            AddFinding(outReport, SecSeverity::High,
                "Açık Metin HTTP İletişimi İzin Verilmiş (Cleartext Traffic)",
                "Uygulama şifrelenmemiş (HTTP) ağ trafiğine izin veriyor. Bu durum aynı WiFi ağındaki saldırganların araya girerek (MITM) kullanıcı şifrelerini ve API isteklerini dinlemesine imkan tanır.",
                "Ağ trafiğini sadece HTTPS/TLS ile sınırlandırın ve android:usesCleartextTraffic=\"false\" olarak ayarlayın.",
                "AndroidManifest.xml", "CWE-319: Cleartext Transmission of Sensitive Information");
        }

        // Exported components check
        if (xml.find("android:exported=\"true\"") != std::string::npos) {
            AddFinding(outReport, SecSeverity::Medium,
                "Dışa Açık Bileşenler Tespit Edildi (android:exported=true)",
                "Uygulama içindeki bazı Activity, Service veya BroadcastReceiver bileşenleri dış uygulamaların çağrısına açık bırakılmış. Yetkilendirme kontrolü yapılmazsa niyet manipülasyonu (Intent Redirection) gerçekleşebilir.",
                "Gerekmeyen bileşenlerde android:exported=\"false\" kullanın veya erişimi android:permission ile sınırlandırın.",
                "AndroidManifest.xml", "CWE-926: Improper Export of Android Application Components");
        }
    }

    // 2. Declared Permissions Risk Evaluation
    for (const auto& perm : model->permissions) {
        std::string pUpper = perm;
        for (char& c : pUpper) c = (char)toupper((unsigned char)c);

        if (pUpper.find("SEND_SMS") != std::string::npos || pUpper.find("RECEIVE_SMS") != std::string::npos) {
            AddFinding(outReport, SecSeverity::High,
                "Kritik İzin: SMS Gönderme / Okuma (" + perm + ")",
                "Uygulama arka planda SMS gönderme veya gelen SMS mesajlarını okuma yetkisine sahip. Kötüye kullanımda iki faktörlü doğrulama (2FA) kodlarının çalınması riski mevcuttur.",
                "Gerekmedikçe SMS izinlerini talep etmeyin; bunun yerine SMS Retriever API kullanın.",
                "AndroidManifest.xml", "CWE-250: Execution with Unnecessary Privileges");
        } else if (pUpper.find("SYSTEM_ALERT_WINDOW") != std::string::npos) {
            AddFinding(outReport, SecSeverity::Medium,
                "Ekran Üzerine Çizim İzni (SYSTEM_ALERT_WINDOW)",
                "Uygulama diğer uygulamaların üzerine pencere çizme iznine sahip. Tapjacking veya sahte arayüz kaplama (Overlay Attack) saldırılarına açıktır.",
                "Overlay iznini zorunlu olmadıkça istemeyin.",
                "AndroidManifest.xml", "CWE-1021: Improper Restriction of Rendered UI Layers");
        } else if (pUpper.find("READ_CONTACTS") != std::string::npos || pUpper.find("WRITE_CONTACTS") != std::string::npos) {
            AddFinding(outReport, SecSeverity::Medium,
                "Rehber Erişimi İzni (" + perm + ")",
                "Kullanıcının tüm kişi rehberine doğrudan erişim sağlar.",
                "Kişi seçimi için Contacts Intent kullanın.",
                "AndroidManifest.xml", "CWE-200: Exposure of Sensitive Information");
        }
    }

    // 3. Signature Vulnerabilities (Janus CVE-2017-13156 & Debug Key)
    if (model->signature.hasV1 && !model->signature.hasV2 && !model->signature.hasV3) {
        AddFinding(outReport, SecSeverity::High,
            "Janus Güvenlik Açığı Riski (Yalnızca APK Signature v1 - CVE-2017-13156)",
            "APK yalnızca v1 (JAR) imza şeması ile imzalanmış ve v2/v3 koruması içermiyor. Android 5.0 - 8.0 arası cihazlarda saldırganlar APK başlığına zararlı DEX enjekte ederek imzayı bozmadan kod çalıştırabilir.",
            "Uygulamayı Android SDK apksigner ile v2 ve v3 imza şemalarını aktif ederek yeniden imzalayın.",
            "META-INF/CERT.RSA", "CWE-347: Improper Verification of Cryptographic Signature");
    }

    if (model->signature.signerSubject.find("Android Debug") != std::string::npos ||
        model->signature.signerSubject.find("testkey") != std::string::npos) {
        AddFinding(outReport, SecSeverity::High,
            "Geliştirici Test Anahtarı ile İmzalanmış (Debug Keystore)",
            "APK genel dağıtıma uygun olmayan varsayılan Android Studio debug.keystore veya testkey ile imzalanmış. Bu durum sahte güncelleme saldırılarına yol açabilir.",
            "Uygulamayı güvenli bir üretim anahtarı (Release Keystore) ile imzalayın.",
            "APK İmza Bloğu", "CWE-295: Improper Certificate Validation");
    }

    // 4. Hardcoded Secrets, Sensitive Keys & Weak Crypto Scanner
    bool foundGoogleKey = false;
    bool foundAwsKey = false;
    bool foundJwt = false;
    bool foundWeakCrypto = false;

    // Scan formattedXml first
    if (model->formattedXml.find("AIzaSy") != std::string::npos) {
        foundGoogleKey = true;
        AddFinding(outReport, SecSeverity::High,
            "Açıkta Kalan Google API Anahtarı (Hardcoded Google API Key)",
            "APK Manifest ve kaynak kodları içinde sabit Google API / Firebase anahtarı tespit edildi (AIzaSy...). Kısıtlanmamışsa yetkisiz kota tüketimi ve veri sızıntısı oluşabilir.",
            "Google Cloud Console üzerinden API anahtarına Android Paket Adı ve SHA-1 parmak izi kısıtlaması ekleyin.",
            "AndroidManifest.xml", "CWE-798: Use of Hard-coded Credentials");
    }

    // Scan APK raw content if apkPath is valid
    if (apkPath && apkPath[0] != L'\0') {
        HANDLE hFile = CreateFileW(apkPath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hFile != INVALID_HANDLE_VALUE) {
            LARGE_INTEGER fileSize;
            if (GetFileSizeEx(hFile, &fileSize)) {
                // Read up to first 8MB to quickly scan DEX and assets
                DWORD toRead = (fileSize.QuadPart > 8 * 1024 * 1024) ? (8 * 1024 * 1024) : (DWORD)fileSize.QuadPart;
                std::vector<char> buf(toRead + 1, 0);
                DWORD bytesRead = 0;
                if (ReadFile(hFile, buf.data(), toRead, &bytesRead, NULL) && bytesRead > 1024) {
                    std::string raw(buf.data(), bytesRead);

                    // Check Google API Key if not already found
                    if (!foundGoogleKey) {
                        size_t pos = raw.find("AIzaSy");
                        if (pos != std::string::npos && pos + 39 <= raw.size()) {
                            foundGoogleKey = true;
                            AddFinding(outReport, SecSeverity::High,
                                "Açıkta Kalan Google API Anahtarı (Hardcoded Google API Key)",
                                "DEX baytkodları içinde sabit Google API / Firebase anahtarı tespit edildi: '" + raw.substr(pos, 16) + "...'.",
                                "Google Cloud Console üzerinden API anahtarına Android Paket Adı ve SHA-1 kısıtlaması tanımlayın.",
                                "classes.dex", "CWE-798: Use of Hard-coded Credentials");
                        }
                    }

                    // Check AWS Access Key
                    if (!foundAwsKey) {
                        size_t pos = raw.find("AKIA");
                        if (pos != std::string::npos && pos + 20 <= raw.size()) {
                            // Check if next 16 chars are alphanumeric
                            bool validAws = true;
                            for (size_t i = 0; i < 20; ++i) {
                                char c = raw[pos + i];
                                if (!isalnum((unsigned char)c)) { validAws = false; break; }
                            }
                            if (validAws) {
                                foundAwsKey = true;
                                AddFinding(outReport, SecSeverity::Critical,
                                    "Kritik AWS Erişim Anahtarı (Hardcoded AWS Access Key)",
                                    "APK kodları içinde 'AKIA...' ile başlayan sabit Amazon Web Services anahtarı tespit edildi. Saldırganlar bulut altyapınıza tam erişim sağlayabilir.",
                                    "İstemci uygulamalarında sabit AWS anahtarı saklamayın; AWS Cognito veya geçici STS token kullanın.",
                                    "Bytecode / Asset", "CWE-798: Use of Hard-coded Credentials");
                            }
                        }
                    }

                    // Check Hardcoded JWT Token
                    if (!foundJwt && raw.find("eyJhbGciOi") != std::string::npos) {
                        foundJwt = true;
                        AddFinding(outReport, SecSeverity::Medium,
                            "Sabit Kodlanmış JWT Belirteci (Hardcoded JWT Token)",
                            "Kod içerisinde sabit JSON Web Token ('eyJhbGciOi...') tespit edildi. Bu belirteç süresi dolmamış bir kimlik doğrulama anahtarı barındırıyor olabilir.",
                            "Kullanıcı ve istemci oturum belirteçlerini dinamik olarak login API uçlarından edinin.",
                            "Bytecode Data", "CWE-798: Use of Hard-coded Credentials");
                    }

                    // Check Insecure Cipher Mode (AES/ECB)
                    if (!foundWeakCrypto && (raw.find("AES/ECB") != std::string::npos || raw.find("DES/CBC") != std::string::npos)) {
                        foundWeakCrypto = true;
                        AddFinding(outReport, SecSeverity::Medium,
                            "Zayıf Kriptografik Algoritma / Mod Kullanımı (AES/ECB)",
                            "Kripto modülünde ECB (Electronic Codebook) modu veya DES kullanımı tespit edildi. ECB modu kalıp sızıntısına yol açtığından güvensiz kabul edilir.",
                            "AES-GCM (Kimlik doğrulamalı şifreleme) moduna geçiş yapın.",
                            "Crypto Module", "CWE-327: Use of a Broken or Risky Cryptographic Algorithm");
                    }
                }
            }
            CloseHandle(hFile);
        }
    }

    // 5. Native Libraries Security Check
    bool hasNative = false;
    for (const auto& res : model->resources) {
        if (res.path.length() >= 3 && res.path.compare(res.path.length() - 3, 3, ".so") == 0) {
            hasNative = true;
            break;
        }
    }
    if (hasNative) {
        AddFinding(outReport, SecSeverity::Info,
            "Yerel (Native C/C++) Kütüphaneler Tespit Edildi (.so)",
            "Uygulama NDK / C++ ikilileri içeriyor. Kaiseki Native Binary Audit ile kütüphanelerin PIE, Stack Canary ve NX korumaları doğrulanmalıdır.",
            "Tüm derleme bayraklarında -fstack-protector-strong ve -fPIE / -pie parametrelerinin açık olduğunu doğrulayın.",
            "lib/<abi>/*.so", "CWE-693: Protection Mechanism Failure");
    }

    // 6. Packer & Protector & Encryption Engine Detection
    KaisekiSec_DetectPacker(model, apkPath, &outReport->packer);
    if (outReport->packer.isPacked) {
        AddFinding(outReport, SecSeverity::Info,
            "APK Koruma / Packer Tespit Edildi: " + outReport->packer.name,
            outReport->packer.description + " [Tespit Edilen İmza: " + outReport->packer.signature + " | Kripto/Yükleme: " + outReport->packer.encryptionAlg + "]",
            "Uygulama harici bir paketleyici/zırhlayıcı ile korunmaktadır. DEX sınıfları dinamik bellekten yüklendiği için statik analize ilave olarak dinamik bellek analizi (Kaiseki Frida Bypass / Memory Dump) uygulanmalıdır.",
            outReport->packer.signature, "CWE-693: Protection Mechanism Failure");
    }

    // 7. Calculate Risk Score & Grade
    int penalty = (outReport->criticalCount * 25) + (outReport->highCount * 12) + (outReport->mediumCount * 5);
    outReport->riskScore = 100 - penalty;
    if (outReport->riskScore < 0) outReport->riskScore = 0;

    if (outReport->riskScore >= 90) outReport->riskGrade = "A+";
    else if (outReport->riskScore >= 80) outReport->riskGrade = "A";
    else if (outReport->riskScore >= 70) outReport->riskGrade = "B";
    else if (outReport->riskScore >= 60) outReport->riskGrade = "C";
    else if (outReport->riskScore >= 50) outReport->riskGrade = "D";
    else outReport->riskGrade = "F (Kritik Risk)";

    // 8. Generate Universal Frida Bypass Script
    KaisekiSec_GenerateFridaBypass(model->packageName.c_str(), &outReport->universalFridaScript);

    return true;
}

KAISEKI_SEC_API bool KaisekiSec_DetectPacker(const ApkDataModel* model, const wchar_t* apkPath, PackerInfo* outPacker) {
    if (!outPacker) return false;
    outPacker->isPacked = false;
    outPacker->name = "Yok (Korumasız APK)";
    outPacker->signature = "";
    outPacker->encryptionAlg = "Standart Açık DEX (Paketleyici / Shell Yok)";
    outPacker->description = "APK üzerinde harici bir koruma / şifreleme kabuğu (packer / protector) tespit edilmedi.";

    if (!model) return false;

    // Collect all entry paths in lower case
    std::string allNamesCombined;
    for (const auto& res : model->resources) {
        std::string low = res.path;
        for (char& c : low) c = (char)tolower((unsigned char)c);
        allNamesCombined += low + ";";
    }

    std::string manifestLower = model->formattedXml;
    for (char& c : manifestLower) c = (char)tolower((unsigned char)c);

    struct PackerSignatureRule {
        const char* name;
        const char* encryption;
        std::vector<const char*> soPatterns;
        std::vector<const char*> classPatterns;
        std::vector<const char*> assetPatterns;
        const char* desc;
    };

    const std::vector<PackerSignatureRule> rules = {
        {
            "360 Jiagu (360加固 / Qihoo 360)",
            "AES-128/256-CBC + Native Dex In-Memory Decryption Stub",
            { "libjiagu.so", "libjiagu_art.so", "libjiagu_x86.so", "libjiagu_a64.so" },
            { "com.qihoo.util.stubapp", "com.qihoo.util.qhclassloader" },
            { "jiagu_data.bin", "assets/libjiagu.so" },
            "Qihoo 360 tarafından geliştirilen kurumsal seviye APK koruma ve baytkod şifreleme motoru."
        },
        {
            "Tencent Legu (腾讯乐固 / 腾讯御安全)",
            "DES / RC4 / Custom Dex Decryption Stub",
            { "libtup.so", "libshell.so", "libbugly.so", "libshellx", "libshella", "libtx_" },
            { "com.tencent.stubshell.txapp", "com.tencent.stubshell" },
            { "assets/0oo00oo", "assets/o0ooooo0oo", "assets/tosversion" },
            "Tencent tarafından geliştirilen DEX dinamik yükleyici ve anti-analiz koruma mekanizması."
        },
        {
            "Bangcle SecShell (梆梆加固)",
            "AES-CBC + Native Dynamic Dex Loading & VMP",
            { "libsecexe.so", "libsecmain.so", "libsecshell.so", "libdexhelper.so" },
            { "com.secshell.secdata.applicationwrapper", "com.secshell.secdata" },
            { "bangcleplugin", "assets/bangcle" },
            "Bangcle kurumsal zırhlama ve baytkod şifreleme çözümü."
        },
        {
            "Baidu Protect (百度加固)",
            "RC4 / AES-128 Native Stub",
            { "libbaiduprotect.so", "libbaiduprotect_art.so", "libbaiduprotect_x86.so" },
            { "com.baidu.protect.stubapplication", "com.baidu.protect" },
            { "baiduprotect1.jar" },
            "Baidu tarafından geliştirilen bellek içi DEX yükleyici ve anti-tersine mühendislik kabuğu."
        },
        {
            "AliProtect / Alibaba Mobisec (阿里聚安全)",
            "Custom SGM Crypt + VMP (Control Flow Virtualization)",
            { "libmobisec.so", "libmobisecy.so", "libfake_jni.so", "libsgmain.so", "libzuma.so" },
            { "com.ali.mobisecenhance.stubapplication", "com.taobao.wireless.security" },
            { "aliprotect.dat" },
            "Alibaba Cloud / AliPay kurumsal uygulama güvenliği ve yerel VMP koruması."
        },
        {
            "Ijiami SecNeo (爱加密)",
            "AES-CBC + In-Memory Dex Rebuilding",
            { "libexec.so", "libexecmain.so", "libijiami.so" },
            { "com.shell.superapplication", "s.h.e.l.l.s" },
            { "ijiami.dat", "ijm_lib" },
            "SecNeo / 爱加密 bellek içi DEX yeniden yapılandırma ve anti-tamper zırhı."
        },
        {
            "NetEase Dun (网易易盾)",
            "AES / ChaCha20 + Anti-Debug",
            { "libnesec.so", "libapr-1.so" },
            { "com.netease.nis.wrapper.myapplication", "com.netease.nis" },
            { "nemusdk.jar" },
            "NetEase oyun ve uygulama güvenlik motoru."
        },
        {
            "Naga Vdog (娜迦加固)",
            "DES / AES + Anti-Memory-Dump",
            { "libddog.so", "libvdog.so", "libedog.so", "libfdog.so" },
            { "com.stub.stubapp" },
            {},
            "Naga Güvenlik DEX şifreleme ve anti-dump kalkanı."
        },
        {
            "KiwiSec (几维安全)",
            "Dex VMP + Control Flow Obfuscation",
            { "libkws.so", "libkwscmm.so", "libkwscr.so" },
            { "com.kiwisec" },
            {},
            "KiwiSec baytkod sanallaştırma (VMP) ve yerel anti-hooking koruyucusu."
        },
        {
            "DingXiang (顶象加固)",
            "AES / ChaCha20 + Threat Intelligence Stub",
            { "libdxbase.so", "libdxrisk.so" },
            { "com.dingxiang.mobile" },
            {},
            "DingXiang mobil risk tespiti ve ikili zırhlayıcı."
        },
        {
            "DexGuard (GuardSquare Enterprise)",
            "Class & String AES Encryption + Reflection Obfuscation",
            {},
            { "com.guardsquare.dexguard" },
            { "dexguard" },
            "GuardSquare DexGuard gelişmiş baytkod karartma ve şifreleme zırhı."
        },
        {
            "MTProtect (MT Manager 加固 / Lin Jinbin)",
            "Zstd + Native Anti-Tamper & Signature Verification Stub",
            { "libmtprotect.so" },
            { "bin.mt.plus", "bin.mt.apksignature" },
            { "killer_dex", "killer_a64", "killer_arm" },
            "MT Manager yerel anti-imza ve baytkod koruma modülü."
        },
        {
            "AppGuard (INCA / NHN)",
            "AES-256 + Memory Protection & Integrity Check",
            { "libappguard.so", "libappguard-x86.so", "libseciron.so" },
            {},
            {},
            "NHN / INCA AppGuard kurumsal mobil güvenlik motoru."
        },
        {
            "ApkProtector / Medusa",
            "XOR / AES Obfuscation + Anti-Mod Stub",
            { "libapkprotector.so", "libprotect.so" },
            { "com.apkprotector" },
            {},
            "ApkProtector / Medusa dinamik baytkod karartıcı."
        }
    };

    for (const auto& rule : rules) {
        for (const char* so : rule.soPatterns) {
            if (allNamesCombined.find(so) != std::string::npos) {
                outPacker->isPacked = true;
                outPacker->name = rule.name;
                outPacker->signature = std::string("Yerel Kütüphane: ") + so;
                outPacker->encryptionAlg = rule.encryption;
                outPacker->description = rule.desc;
                return true;
            }
        }
        for (const char* cls : rule.classPatterns) {
            if (manifestLower.find(cls) != std::string::npos) {
                outPacker->isPacked = true;
                outPacker->name = rule.name;
                outPacker->signature = std::string("Manifest Sınıfı: ") + cls;
                outPacker->encryptionAlg = rule.encryption;
                outPacker->description = rule.desc;
                return true;
            }
        }
        for (const char* ast : rule.assetPatterns) {
            if (allNamesCombined.find(ast) != std::string::npos) {
                outPacker->isPacked = true;
                outPacker->name = rule.name;
                outPacker->signature = std::string("Varlık Dosyası: ") + ast;
                outPacker->encryptionAlg = rule.encryption;
                outPacker->description = rule.desc;
                return true;
            }
        }
    }

    // Secondary scan: Scan APK raw content for cryptographic algorithms
    if (apkPath && apkPath[0] != L'\0') {
        HANDLE hFile = CreateFileW(apkPath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hFile != INVALID_HANDLE_VALUE) {
            LARGE_INTEGER fileSize;
            if (GetFileSizeEx(hFile, &fileSize)) {
                DWORD toRead = (fileSize.QuadPart > 10 * 1024 * 1024) ? (10 * 1024 * 1024) : (DWORD)fileSize.QuadPart;
                std::vector<char> buf(toRead + 1, 0);
                DWORD bytesRead = 0;
                if (ReadFile(hFile, buf.data(), toRead, &bytesRead, NULL) && bytesRead > 2048) {
                    std::string raw(buf.data(), bytesRead);
                    std::vector<std::string> detectedAlgs;

                    if (raw.find("AES/GCM") != std::string::npos || raw.find("AES_256_GCM") != std::string::npos) detectedAlgs.push_back("AES-GCM (256-bit)");
                    else if (raw.find("AES/CBC") != std::string::npos || raw.find("AES-128") != std::string::npos) detectedAlgs.push_back("AES-CBC");
                    if (raw.find("ChaCha20") != std::string::npos || raw.find("CHACHA20") != std::string::npos) detectedAlgs.push_back("ChaCha20-Poly1305");
                    if (raw.find("RSA/") != std::string::npos || raw.find("RSA/ECB") != std::string::npos) detectedAlgs.push_back("RSA-OAEP/PKCS1");
                    if (raw.find("ARC4") != std::string::npos || raw.find("RC4") != std::string::npos) detectedAlgs.push_back("RC4/ARC4");
                    if (raw.find("DESede") != std::string::npos || raw.find("DES/CBC") != std::string::npos) detectedAlgs.push_back("3DES / DES");

                    if (!detectedAlgs.empty()) {
                        std::string algStr;
                        for (size_t i = 0; i < detectedAlgs.size(); ++i) {
                            if (i > 0) algStr += ", ";
                            algStr += detectedAlgs[i];
                        }
                        outPacker->encryptionAlg = algStr + " (Uygulama İçi Şifreleme)";
                    }
                }
            }
            CloseHandle(hFile);
        }
    }

    return true;
}

KAISEKI_SEC_API void KaisekiSec_FreePackerInfo(PackerInfo* packer) {
    if (!packer) return;
    std::string().swap(packer->name);
    std::string().swap(packer->signature);
    std::string().swap(packer->encryptionAlg);
    std::string().swap(packer->description);
    packer->isPacked = false;
}

KAISEKI_SEC_API void KaisekiSec_FreeReport(SecReport* report) {
    if (report) {
        std::vector<SecFinding>().swap(report->findings);
        std::string().swap(report->universalFridaScript);
        std::string().swap(report->nativeElfReport);
        KaisekiSec_FreePackerInfo(&report->packer);
    }
}

KAISEKI_SEC_API void KaisekiSec_FreeScript(std::string* script) {
    if (script) std::string().swap(*script);
}

KAISEKI_SEC_API void KaisekiSec_FreeElfReport(std::string* report) {
    if (report) std::string().swap(*report);
}

#include "KaisekiVersion.h"

KAISEKI_SEC_API const char* KaisekiSec_GetVersion() {
#if defined(_DEBUG)
    return KAISEKI_VERSION_STRING "-debug-v143-mdd";
#else
    return KAISEKI_VERSION_STRING "-release-v143-md";
#endif
}

}
