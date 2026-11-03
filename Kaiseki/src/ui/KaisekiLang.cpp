#include "KaisekiLang.h"
#include <fstream>
#include <sstream>
#include <algorithm>

const char* KaisekiLang::g_keyNames[] = {
    // Menu Bar
    "Menu_File",
    "Menu_Inspect",
    "Menu_Tools",
    "Menu_View",
    "Menu_Settings",
    "Menu_About",

    // File Menu
    "Menu_OpenApk",
    "Menu_OpenKproj",
    "Menu_SaveKproj",
    "Menu_SaveTab",
    "Menu_ExportGradle",
    "Menu_RepackApk",
    "Menu_Exit",

    // Inspect Menu
    "Menu_InspectManifest",
    "Menu_InspectSignature",
    "Menu_InspectSummary",
    "Menu_InspectClasses",
    "Menu_InspectArsc",
    "Menu_InspectSecurity",

    // Tools Menu
    "Menu_DiffApk",
    "Menu_AdbDevices",
    "Menu_ApkSigner",
    "Menu_DeobfToggle",
    "Menu_LoadMapping",

    // View Menu
    "Menu_ViewExplorer",
    "Menu_ViewSigner",
    "Menu_ViewAdb",
    "Menu_ViewSettings",
    "Menu_ViewTerminal",

    // Rail Tooltips
    "Rail_Explorer",
    "Rail_Signer",
    "Rail_Adb",
    "Rail_Settings",

    // Explorer Tree
    "Tree_ProjectExplorer",
    "Tree_SourceCode",
    "Tree_Resources",
    "Tree_Signatures",
    "Tree_NoFileOpen",

    // Status Bar
    "Status_Ready",
    "Status_LoadingApk",
    "Status_ClassesCount",
    "Status_Standalone",

    // Terminal & Logcat
    "Tab_Terminal",
    "Tab_Logcat",
    "Btn_Refresh",
    "Btn_Clear",
    "Terminal_Welcome",

    // Settings Navigation (12 Categories)
    "Settings_Title",
    "Settings_TabGeneral",
    "Settings_TabTheme",
    "Settings_TabLanguage",
    "Settings_TabTools",
    "Settings_TabDecompile",
    "Settings_TabEditor",
    "Settings_TabBuild",
    "Settings_TabAdb",
    "Settings_TabSecurity",
    "Settings_TabPerformance",
    "Settings_TabHotkeys",
    "Settings_TabAdvanced",

    // Settings Language Tab
    "Settings_LangTitle",
    "Settings_LangDesc",
    "Settings_LangTurkishName",
    "Settings_LangTurkishDesc",
    "Settings_LangEnglishName",
    "Settings_LangEnglishDesc",
    "Settings_LangSelected",
    "Settings_LangSelectBtn",
    "Settings_OpenLangFolder",
    "Settings_RefreshLangs",
    "Settings_LangFolderHint",

    // Settings Theme Tab
    "Settings_ThemePaletteTitle",
    "Settings_ThemeSlidersTitle",
    "Settings_SliderR",
    "Settings_SliderG",
    "Settings_SliderB",

    // Settings General Tab
    "Settings_TempWorkspaceTitle",
    "Settings_ConfigFileTitle",
    "Settings_FileAssocTitle",
    "Settings_BtnRegisterKproj",

    // Settings Tools Tab
    "Settings_EngineTitle",
    "Settings_EngineDesc",
    "Settings_AdbTitle",
    "Settings_AdbDesc",
    "Settings_BtnExportGradle",
    "Settings_DeobfActive",
    "Settings_DeobfInactive",

    // Settings Decompilation Tab
    "Settings_DecompTitle",
    "Settings_DefaultViewLabel",
    "Settings_FontSizeLabel",

    // Progress Dialog
    "Progress_Title",
    "Progress_Waiting",
    "Progress_Completed",
    "Progress_ZipScan",
    "Progress_DexAxml",
    "Progress_Workspace",
    "Progress_Footer",

    // Signer & ADB View Titles
    "Signer_Title",
    "Adb_Title",

    // Toasts
    "Toast_ClassDecompiled",
    "Toast_NoApkOpen",
    "Toast_GradleExportSuccess",
    "Toast_GradleExportFail",
    "Toast_RepackFail",
    "Toast_RepackSignSuccess",
    "Toast_SignFail",
    "Toast_DiffComplete",
    "Toast_DiffFail",
    "Toast_KprojSaved",
    "Toast_KprojLoaded",
    "Toast_KprojReadFail",
    "Toast_FileSaved",
    "Toast_ApkSuccess",
    "Toast_ApkOpenFail",
    "Toast_PeComplete",
    "Toast_PeFail",
    "Toast_OnlyValidExtensions",
    "Toast_DeobfEnabled",
    "Toast_DeobfDisabled",
    "Toast_KprojAssocSuccess",
    "Toast_ColorSelected",
    "Toast_LogcatRefreshed",
    "Toast_LogcatCleared",
    "Toast_FridaScriptGen",
    "Toast_AdbInstallSuccess",
    "Toast_AdbInstallFail",
    "Toast_AdbLaunchSuccess",
    "Toast_AdbLaunchFail",
    "Toast_LangChanged"
};

const wchar_t* KaisekiLang::g_builtInStrings[][2] = {
    // Menu Bar
    { L"Dosya", L"File" },                               // Menu_File
    { L"İncele", L"Inspect" },                           // Menu_Inspect
    { L"Araçlar", L"Tools" },                            // Menu_Tools
    { L"Görünüm", L"View" },                             // Menu_View
    { L"Ayarlar", L"Settings" },                         // Menu_Settings
    { L"Hakkında", L"About" },                           // Menu_About

    // File Menu
    { L"📂  APK Dosyası Aç... (Ctrl+O)", L"📂  Open APK File... (Ctrl+O)" },     // Menu_OpenApk
    { L"📋  Kaiseki Projesi (.kproj) Aç...", L"📋  Open Kaiseki Project (.kproj)..." }, // Menu_OpenKproj
    { L"💾  Projeyi Kaydet (.kproj) (Ctrl+S)", L"💾  Save Project (.kproj) (Ctrl+S)" }, // Menu_SaveKproj
    { L"📄  Aktif Sekmeyi Farklı Kaydet...", L"📄  Save Current Tab As..." },   // Menu_SaveTab
    { L"📦  Gradle Projesi Olarak Dışa Aktar...", L"📦  Export as Gradle Project..." }, // Menu_ExportGradle
    { L"⚡  APK Yeniden Paketle & İmzala...", L"⚡  Repackage & Sign APK..." }, // Menu_RepackApk
    { L"❌  Çıkış", L"❌  Exit" },                                           // Menu_Exit

    // Inspect Menu
    { L"📜  AndroidManifest.xml Çözümlemesi", L"📜  AndroidManifest.xml Analysis" }, // Menu_InspectManifest
    { L"🔏  APK İmza & X.509 Sertifika Bilgileri", L"🔏  APK Signature & X.509 Certificate" }, // Menu_InspectSignature
    { L"📊  APK Genel Metrik Özeti", L"📊  APK General Summary" },            // Menu_InspectSummary
    { L"☕  Dalvik Bytecode & Smali Sınıfları", L"☕  Dalvik Bytecode & Smali Classes" }, // Menu_InspectClasses
    { L"🗃️  ARSC Kaynak Tablosu (resources.arsc)", L"🗃️  ARSC Resource Table (resources.arsc)" }, // Menu_InspectArsc
    { L"🛡️  Güvenlik & Zafiyet Denetim Raporu", L"🛡️  Security & Vulnerability Audit Report" }, // Menu_InspectSecurity

    // Tools Menu
    { L"⚖️  İkili APK Karşılaştırma (Diff)...", L"⚖️  Binary APK Diff..." }, // Menu_DiffApk
    { L"📱  ADB Cihaz Yöneticisi & Logcat...", L"📱  ADB Device Manager & Logcat..." }, // Menu_AdbDevices
    { L"✒️  APK İmzalama ve Doğrulama Sihirbazı...", L"✒️  APK Signer & Verifier Wizard..." }, // Menu_ApkSigner
    { L"🔤  Otomatik İsimlendirme (Deobfuscate)", L"🔤  Deobfuscation (JOBF)" }, // Menu_DeobfToggle
    { L"🗺️  ProGuard / R8 Mapping Haritası Yükle...", L"🗺️  Load ProGuard / R8 Mapping..." }, // Menu_LoadMapping

    // View Menu
    { L"📁  Proje Gezgini Görünümü", L"📁  Project Explorer View" },         // Menu_ViewExplorer
    { L"✒️  İmzalayıcı Görünümü", L"✒️  Signer View" },                     // Menu_ViewSigner
    { L"📱  ADB & Cihaz Görünümü", L"📱  ADB & Device View" },              // Menu_ViewAdb
    { L"⚙️  Ayarlar ve Tercihler", L"⚙️  Settings and Preferences" },       // Menu_ViewSettings
    { L"💻  Terminal & Log Paneli", L"💻  Terminal & Log Panel" },           // Menu_ViewTerminal

    // Rail Tooltips
    { L"Proje Gezgini (APK)", L"Project Explorer (APK)" },                   // Rail_Explorer
    { L"APK İmzalama & Doğrulama", L"APK Signing & Verification" },          // Rail_Signer
    { L"ADB & Cihaz Yönetimi", L"ADB & Device Management" },                 // Rail_Adb
    { L"Ayarlar & Tercihler", L"Settings & Preferences" },                   // Rail_Settings

    // Explorer Tree
    { L"PROJE GEZGİNİ (APK)", L"PROJECT EXPLORER (APK)" },                   // Tree_ProjectExplorer
    { L"Kaynak Kodları", L"Source Code" },                                   // Tree_SourceCode
    { L"Kaynaklar", L"Resources" },                                          // Tree_Resources
    { L"Sertifika & İmzalar (Signatures)", L"Signatures & Certificates" },   // Tree_Signatures
    { L"Açık bir dosya yok.", L"No open file." },                            // Tree_NoFileOpen

    // Status Bar
    { L"Hazır", L"Ready" },                                                  // Status_Ready
    { L"APK Dosyası Çözümleniyor...", L"Analyzing APK File..." },            // Status_LoadingApk
    { L"Multidex (%d Sınıf)", L"Multidex (%d Classes)" },                    // Status_ClassesCount
    { L"x64 Yerel Motor", L"x64 Native Engine" },                            // Status_Standalone

    // Terminal & Logcat
    { L"Kaiseki Terminal", L"Kaiseki Terminal" },                            // Tab_Terminal
    { L"Android Logcat", L"Android Logcat" },                                // Tab_Logcat
    { L"Yenile", L"Refresh" },                                               // Btn_Refresh
    { L"Temizle", L"Clear" },                                                // Btn_Clear
    { L"[BİLGİ] Kaiseki Dalvik Ayrıştırma Motoru hazır. APK dosyasını buraya sürükleyin.",
      L"[INFO] Kaiseki Dalvik Disassembler Engine ready. Drag and drop an APK file here." }, // Terminal_Welcome

    // Settings Navigation (7 Consolidated Categories)
    { L"Ayarlar ve Tercihler", L"Settings and Preferences" },               // Settings_Title
    { L"⚙️ Genel", L"⚙️ General" },                                         // Settings_TabGeneral
    { L"🎨 Görünüm", L"🎨 Appearance" },                                    // Settings_TabTheme
    { L"🌐 Dil", L"🌐 Language" },                                          // Settings_TabLanguage
    { L"🛠️ Android Araçları", L"🛠️ Android Tools" },                       // Settings_TabTools
    { L"📦 APK İşlemleri", L"📦 APK Operations" },                           // Settings_TabDecompile
    { L"📝 Kod Editörü", L"📝 Code Editor" },                               // Settings_TabEditor (kept for string table alignment)
    { L"🔨 APK Paketleme & İmza", L"🔨 APK Build & Sign" },                 // Settings_TabBuild (kept for string table alignment)
    { L"📱 Cihazlar & ADB", L"📱 Devices & ADB" },                          // Settings_TabAdb (kept for string table alignment)
    { L"🛡️ Güvenlik & Gizlilik", L"🛡️ Security & Privacy" },               // Settings_TabSecurity (kept for string table alignment)
    { L"⚡ Performans & Bellek", L"⚡ Performance & Memory" },              // Settings_TabPerformance (kept for string table alignment)
    { L"⌨️ Kısayollar", L"⌨️ Keybindings" },                                 // Settings_TabHotkeys
    { L"🔬 Gelişmiş ve Tanılama", L"🔬 Advanced & Diagnostics" },           // Settings_TabAdvanced

    // Settings Language Tab
    { L"Uygulama Arayüz Dili:", L"Application Interface Language:" },        // Settings_LangTitle
    { L"Arayüz metinleri, menüler, sekmeler ve bildirimler anında seçilen dile çevrilir.",
      L"Interface texts, menus, tabs, and notifications are instantly translated to the selected language." }, // Settings_LangDesc
    { L"Türkçe (Turkish)", L"Turkish (Türkçe)" },                            // Settings_LangTurkishName
    { L"Varsayılan sistem arayüz dili (Türkçe).", L"Default interface language (Turkish)." }, // Settings_LangTurkishDesc
    { L"English (İngilizce)", L"English" },                                  // Settings_LangEnglishName
    { L"Full English interface for international development.",
      L"Full English interface for international development." },             // Settings_LangEnglishDesc
    { L"✓ Seçili Dil", L"✓ Current Language" },                              // Settings_LangSelected
    { L"Bu Dili Seç", L"Select Language" },                                  // Settings_LangSelectBtn
    { L"📂  Dil Klasörünü Aç (Explorer)", L"📂  Open Language Folder (Explorer)" }, // Settings_OpenLangFolder
    { L"🔄  Dilleri Yenile", L"🔄  Refresh Languages" },                     // Settings_RefreshLangs
    { L"💡 Profesyonel Dil Sistemi: Yeni bir dil eklemek için lang/ klasörüne yeni bir .xml dil dosyası ekleyebilirsiniz.",
      L"💡 Professional Language System: To add a new language, simply place a new .xml file into the lang/ directory." }, // Settings_LangFolderHint

    // Settings Theme Tab
    { L"1. Özel Vurgu Renk Paleti (Varsayılan: Sakura Pastel Pembe):",
      L"1. Accent Color Palette (Default: Sakura Pastel Pink):" },           // Settings_ThemePaletteTitle
    { L"2. Dahili RGB Renk Ayarı:", L"2. Custom RGB Color Adjustment:" },    // Settings_ThemeSlidersTitle
    { L"Kırmızı (R)", L"Red (R)" },                                          // Settings_SliderR
    { L"Yeşil (G)", L"Green (G)" },                                          // Settings_SliderG
    { L"Mavi (B)", L"Blue (B)" },                                            // Settings_SliderB

    // Settings General Tab
    { L"Geçici Çalışma Alanı (Temp Klasörü):", L"Temporary Workspace Directory:" }, // Settings_TempWorkspaceTitle
    { L"Yapılandırma Dosyası (%APPDATA% XML Kaydı):", L"Configuration File (%APPDATA% XML):" }, // Settings_ConfigFileTitle
    { L"Windows .kproj Dosya İlişkilendirmesi:", L"Windows .kproj File Association:" }, // Settings_FileAssocTitle
    { L"🔗  .kproj İlişkilendirmesini Şimdi Kaydet", L"🔗  Register .kproj File Association Now" }, // Settings_BtnRegisterKproj

    // Settings Tools Tab
    { L"Yerel Ayrıştırma & Dalvik Motoru (KaisekiDex.dll):",
      L"Native Decompiler & Dalvik Engine (KaisekiDex.dll):" },              // Settings_EngineTitle
    { L"Kaiseki DexEngine (Saf C++ DLL • Sıfır Java/JRE Bağımlılığı • %100 Yerel Aktif)",
      L"Kaiseki DexEngine (Pure C++ DLL • Zero Java/JRE Dependency • 100% Native Active)" }, // Settings_EngineDesc
    { L"Android Debug Bridge (adb.exe):", L"Android Debug Bridge (adb.exe):" }, // Settings_AdbTitle
    { L"Sistem PATH üzerinden 'adb' komutu hazır durumda.", L"The 'adb' command is available via system PATH." }, // Settings_AdbDesc
    { L"📦  Android Studio Gradle Projesi Çıkar", L"📦  Export Android Studio Gradle Project" }, // Settings_BtnExportGradle
    { L"✓ İsimlendirme Çözümleme (JOBF) Aktif", L"✓ Deobfuscation (JOBF) Active" }, // Settings_DeobfActive
    { L"✕ İsimlendirme Çözümleme (Kapalı)", L"✕ Deobfuscation (Disabled)" },        // Settings_DeobfInactive

    // Settings Decompilation Tab
    { L"Dalvik Dekompilasyon Motor Ayarları:", L"Dalvik Decompilation Engine Settings:" }, // Settings_DecompTitle
    { L"Varsayılan Kod Görünümü:", L"Default Code View:" },                  // Settings_DefaultViewLabel
    { L"Editör Yazı Tipi Boyutu (Font Size):", L"Editor Font Size:" },        // Settings_FontSizeLabel

    // Progress Dialog
    { L"İşlem Yürütülüyor...", L"Processing..." },                           // Progress_Title
    { L"Lütfen bekleyin...", L"Please wait..." },                            // Progress_Waiting
    { L"Tamamlandı!", L"Completed!" },                                       // Progress_Completed
    { L"PKZIP arşivi taranıyor...", L"Scanning PKZIP archive..." },          // Progress_ZipScan
    { L"Dalvik DEX ve AXML çözümleniyor...", L"Parsing Dalvik DEX and AXML..." }, // Progress_DexAxml
    { L"Smali & Java çalışma alanı hazırlanıyor...", L"Preparing Smali & Java workspace..." }, // Progress_Workspace
    { L"Kaiseki Native Engine • Zero External Dependencies • Yüksek Hızlı Donanım Ayrıştırması",
      L"Kaiseki Native Engine • Zero External Dependencies • High-Speed Hardware Acceleration" }, // Progress_Footer

    // Signer & ADB View Titles
    { L"APK İmzalama ve Doğrulama", L"APK Signing and Verification" },       // Signer_Title
    { L"Telefona Kurulum & ADB Dağıtımı", L"Device Install & ADB Deployment" }, // Adb_Title

    // Toasts
    { L"[BİLGİ] Sınıf başarıyla decompile edildi: ", L"[INFO] Class successfully decompiled: " }, // Toast_ClassDecompiled
    { L"[UYARI] Lütfen önce bir APK dosyası açın!", L"[WARNING] Please open an APK file first!" }, // Toast_NoApkOpen
    { L"[BAŞARILI] Android Studio Gradle projesi başarıyla kaydedildi!",
      L"[SUCCESS] Android Studio Gradle project successfully exported!" }, // Toast_GradleExportSuccess
    { L"[HATA] Gradle projesi dışa aktarılamadı!", L"[ERROR] Failed to export Gradle project!" }, // Toast_GradleExportFail
    { L"[HATA] Paketleme başarısız: ", L"[ERROR] Repackaging failed: " },      // Toast_RepackFail
    { L"[BAŞARILI] APK başarıyla paketlendi ve imzalandı!",
      L"[SUCCESS] APK successfully repackaged and signed!" },                 // Toast_RepackSignSuccess
    { L"[HATA] İmzalama başarısız oldu: ", L"[ERROR] Signing failed: " },     // Toast_SignFail
    { L"[BİLGİ] APK Karşılaştırma tamamlandı: ", L"[INFO] APK Diff completed: " }, // Toast_DiffComplete
    { L"[HATA] APK karşılaştırma işlemi başarısız oldu!", L"[ERROR] APK Diff failed!" }, // Toast_DiffFail
    { L"[BAŞARILI] Kaiseki projesi (.kproj) kaydedildi!", L"[SUCCESS] Kaiseki project (.kproj) saved!" }, // Toast_KprojSaved
    { L"[BAŞARILI] Kaiseki projesi başarıyla yüklendi!", L"[SUCCESS] Kaiseki project loaded successfully!" }, // Toast_KprojLoaded
    { L"[HATA] Proje dosyası okunamadı!", L"[ERROR] Failed to read project file!" }, // Toast_KprojReadFail
    { L"[BİLGİ] Sekme kaydedildi: ", L"[INFO] Tab saved: " },                 // Toast_FileSaved
    { L"[BAŞARILI] APK başarıyla çözümlendi!", L"[SUCCESS] APK analyzed successfully!" }, // Toast_ApkSuccess
    { L"[HATA] APK dosyası açılamadı veya bozuk!", L"[ERROR] Could not open APK file or file corrupted!" }, // Toast_ApkOpenFail
    { L"[BİLGİ] PE analizi tamamlandı: ", L"[INFO] PE analysis completed: " }, // Toast_PeComplete
    { L"[HATA] PE dosyası çözümlenemedi!", L"[ERROR] Failed to analyze PE file!" }, // Toast_PeFail
    { L"[UYARI] Yalnızca .apk, .kproj, .exe ve .dll uzantılı dosyalar kabul edilir!",
      L"[WARNING] Only .apk, .kproj, .exe, and .dll files are accepted!" },    // Toast_OnlyValidExtensions
    { L"[BİLGİ] Otomatik isimlendirme (JOBF) açıldı!", L"[INFO] Deobfuscation (JOBF) enabled!" }, // Toast_DeobfEnabled
    { L"[BİLGİ] Otomatik isimlendirme (JOBF) kapatıldı!", L"[INFO] Deobfuscation (JOBF) disabled!" }, // Toast_DeobfDisabled
    { L"[BAŞARILI] .kproj dosya ilişkilendirmesi başarıyla kaydedildi!",
      L"[SUCCESS] .kproj file association registered successfully!" },         // Toast_KprojAssocSuccess
    { L"[BİLGİ] Renk teması güncellendi: ", L"[INFO] Color theme updated: " }, // Toast_ColorSelected
    { L"[BİLGİ] Logcat güncellendi.", L"[INFO] Logcat refreshed." },           // Toast_LogcatRefreshed
    { L"[BİLGİ] Logcat temizlendi.", L"[INFO] Logcat cleared." },             // Toast_LogcatCleared
    { L"[BİLGİ] Frida hook scripti üretildi: ", L"[INFO] Frida hook script generated: " }, // Toast_FridaScriptGen
    { L"[BAŞARILI] APK telefona başarıyla yüklendi! (Success)",
      L"[SUCCESS] APK successfully installed on device!" },                   // Toast_AdbInstallSuccess
    { L"[HATA] Telefona kurulum başarısız oldu!", L"[ERROR] Device installation failed!" }, // Toast_AdbInstallFail
    { L"[BAŞARILI] Uygulama telefonda başlatıldı!", L"[SUCCESS] Application launched on device!" }, // Toast_AdbLaunchSuccess
    { L"[HATA] Uygulama başlatılamadı!", L"[ERROR] Failed to launch application on device!" }, // Toast_AdbLaunchFail
    { L"[BİLGİ] Arayüz dili değiştirildi / Language updated.",
      L"[INFO] Interface language updated / Arayüz dili değiştirildi." }       // Toast_LangChanged
};

// Helper: Convert UTF-8 to UTF-16 std::wstring
static std::wstring Utf8ToWide(const std::string& str) {
    if (str.empty()) return L"";
    int count = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.length(), NULL, 0);
    if (count <= 0) return L"";
    std::wstring result(count, 0);
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.length(), &result[0], count);
    return result;
}

// Helper: Unescape standard XML entities
static std::string UnescapeXml(const std::string& input) {
    std::string s = input;
    size_t pos = 0;
    while ((pos = s.find("&quot;", pos)) != std::string::npos) { s.replace(pos, 6, "\""); pos += 1; }
    pos = 0;
    while ((pos = s.find("&apos;", pos)) != std::string::npos) { s.replace(pos, 6, "'"); pos += 1; }
    pos = 0;
    while ((pos = s.find("&lt;", pos)) != std::string::npos) { s.replace(pos, 4, "<"); pos += 1; }
    pos = 0;
    while ((pos = s.find("&gt;", pos)) != std::string::npos) { s.replace(pos, 4, ">"); pos += 1; }
    pos = 0;
    while ((pos = s.find("&amp;", pos)) != std::string::npos) { s.replace(pos, 5, "&"); pos += 1; }
    return s;
}

static std::string EscapeXml(const std::string& input) {
    std::string s = "";
    for (char c : input) {
        if (c == '&') s += "&amp;";
        else if (c == '<') s += "&lt;";
        else if (c == '>') s += "&gt;";
        else if (c == '"') s += "&quot;";
        else s += c;
    }
    return s;
}

KaisekiLang::KaisekiLang() {
    m_activeStrings.resize((size_t)StringId::Count_);
    for (size_t i = 0; i < (size_t)StringId::Count_; ++i) {
        m_activeStrings[i] = g_builtInStrings[i][0];
    }
    ScanLanguages();
}

const char* KaisekiLang::GetKeyName(StringId id) {
    size_t idx = (size_t)id;
    if (idx < (size_t)StringId::Count_) {
        return g_keyNames[idx];
    }
    return "";
}

bool KaisekiLang::GetIdByKey(const std::string& key, StringId& outId) {
    for (size_t i = 0; i < (size_t)StringId::Count_; ++i) {
        if (key == g_keyNames[i]) {
            outId = (StringId)i;
            return true;
        }
    }
    return false;
}

std::wstring KaisekiLang::GetLangDirectory() const {
    wchar_t exePath[MAX_PATH] = { 0 };
    GetModuleFileNameW(NULL, exePath, MAX_PATH);
    wchar_t* lastSlash = wcsrchr(exePath, L'\\');
    std::wstring exeDir = lastSlash ? std::wstring(exePath, lastSlash - exePath) : L".";

    // 1. Check exeDir\lang
    std::wstring p1 = exeDir + L"\\lang";
    DWORD attr1 = GetFileAttributesW(p1.c_str());
    if (attr1 != INVALID_FILE_ATTRIBUTES && (attr1 & FILE_ATTRIBUTE_DIRECTORY)) {
        return p1;
    }

    // 2. Check current working directory .\lang
    std::wstring p2 = L"lang";
    DWORD attr2 = GetFileAttributesW(p2.c_str());
    if (attr2 != INVALID_FILE_ATTRIBUTES && (attr2 & FILE_ATTRIBUTE_DIRECTORY)) {
        return p2;
    }

    // 3. Check ..\..\lang (Development workspace root when running from bin\x64)
    std::wstring p3 = exeDir + L"\\..\\..\\lang";
    DWORD attr3 = GetFileAttributesW(p3.c_str());
    if (attr3 != INVALID_FILE_ATTRIBUTES && (attr3 & FILE_ATTRIBUTE_DIRECTORY)) {
        return p3;
    }

    return p1;
}

bool KaisekiLang::ParseLanguageHeader(const std::wstring& filePath, LanguagePackInfo& outInfo) {
    std::ifstream file(filePath, std::ios::binary);
    if (!file.is_open()) return false;

    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    file.close();

    // Look for <Language ...>
    size_t tagPos = content.find("<Language");
    if (tagPos == std::string::npos) return false;
    size_t tagEnd = content.find(">", tagPos);
    if (tagEnd == std::string::npos) return false;

    std::string tag = content.substr(tagPos, tagEnd - tagPos + 1);

    auto getAttr = [&](const std::string& attr) -> std::string {
        size_t p = tag.find(attr + "=\"");
        if (p != std::string::npos) {
            p += attr.length() + 2;
            size_t e = tag.find("\"", p);
            if (e != std::string::npos) {
                return tag.substr(p, e - p);
            }
        }
        return "";
    };

    outInfo.code = getAttr("code");
    if (outInfo.code.empty()) {
        // Derive code from filename (e.g. tr.xml -> tr)
        size_t sl = filePath.find_last_of(L"\\/");
        std::wstring fname = (sl != std::wstring::npos) ? filePath.substr(sl + 1) : filePath;
        size_t dot = fname.find_last_of(L'.');
        if (dot != std::wstring::npos) fname = fname.substr(0, dot);
        for (wchar_t wc : fname) {
            if (wc < 128) outInfo.code += (char)wc;
        }
    }

    outInfo.name = Utf8ToWide(getAttr("name"));
    outInfo.displayName = Utf8ToWide(getAttr("displayName"));
    outInfo.author = Utf8ToWide(getAttr("author"));
    outInfo.filePath = filePath;
    outInfo.isBuiltIn = false;

    if (outInfo.displayName.empty()) {
        outInfo.displayName = outInfo.name.empty() ? Utf8ToWide(outInfo.code) : outInfo.name;
    }

    // Strip any leading emoji flags (e.g. regional indicator symbols U+1F1E6..U+1F1FF) so clean vector flag shows
    while (!outInfo.displayName.empty() && 
           ((outInfo.displayName[0] >= 0xD800 && outInfo.displayName[0] <= 0xDBFF) ||
            outInfo.displayName[0] == L' ' || outInfo.displayName[0] == L'\t')) {
        if (outInfo.displayName[0] >= 0xD800 && outInfo.displayName[0] <= 0xDBFF && outInfo.displayName.size() >= 2) {
            outInfo.displayName.erase(0, 2);
        } else {
            outInfo.displayName.erase(0, 1);
        }
    }

    return true;
}

bool KaisekiLang::LoadLanguageXml(const std::wstring& filePath, std::vector<std::wstring>& outStrings) {
    std::ifstream file(filePath, std::ios::binary);
    if (!file.is_open()) return false;

    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    file.close();

    size_t pos = 0;
    while ((pos = content.find("<String", pos)) != std::string::npos) {
        // Ensure this is <String ...> and not <Strings...>
        if (pos + 7 < content.size() && content[pos + 7] != ' ' && content[pos + 7] != '\t' && content[pos + 7] != '\r' && content[pos + 7] != '\n') {
            pos += 7;
            continue;
        }
        size_t tagEnd = content.find(">", pos);
        if (tagEnd == std::string::npos) break;

        std::string tagHeader = content.substr(pos, tagEnd - pos + 1);

        // Find id="..."
        std::string strId = "";
        size_t idPos = tagHeader.find("id=\"");
        if (idPos != std::string::npos) {
            idPos += 4;
            size_t idEnd = tagHeader.find("\"", idPos);
            if (idEnd != std::string::npos) {
                strId = tagHeader.substr(idPos, idEnd - idPos);
            }
        }

        // Check if self-closing <String id="..." text="..." />
        std::string valStr = "";
        if (tagHeader.size() >= 2 && tagHeader[tagHeader.size() - 2] == '/') {
            size_t textPos = tagHeader.find("text=\"");
            if (textPos != std::string::npos) {
                textPos += 6;
                size_t textEnd = tagHeader.find("\"", textPos);
                if (textEnd != std::string::npos) {
                    valStr = tagHeader.substr(textPos, textEnd - textPos);
                }
            }
            pos = tagEnd + 1;
        } else {
            // Find closing </String>
            size_t closePos = content.find("</String>", tagEnd);
            if (closePos != std::string::npos) {
                valStr = content.substr(tagEnd + 1, closePos - (tagEnd + 1));
                pos = closePos + 9;
            } else {
                pos = tagEnd + 1;
            }
        }

        if (!strId.empty()) {
            StringId sId;
            if (GetIdByKey(strId, sId)) {
                std::string unescaped = UnescapeXml(valStr);
                outStrings[(size_t)sId] = Utf8ToWide(unescaped);
            }
        }
    }

    return true;
}

void KaisekiLang::EnsureDefaultLanguageFiles(const std::wstring& langDir) {
    CreateDirectoryW(langDir.c_str(), NULL);

    std::wstring trPath = langDir + L"\\tr.xml";
    std::wstring enPath = langDir + L"\\en.xml";

    // Create tr.xml if not existing
    if (GetFileAttributesW(trPath.c_str()) == INVALID_FILE_ATTRIBUTES) {
        std::ofstream trFile(trPath, std::ios::binary | std::ios::trunc);
        if (trFile.is_open()) {
            trFile << "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n";
            trFile << "<!-- Kaiseki - Professional Language Definition File -->\n";
            trFile << "<Language code=\"tr\" name=\"Türkçe\" displayName=\"Türkçe\" author=\"anonimbiri\" version=\"2.5\">\n";
            trFile << "    <Strings>\n";
            for (size_t i = 0; i < (size_t)StringId::Count_; ++i) {
                const wchar_t* wTr = g_builtInStrings[i][0];
                int len = WideCharToMultiByte(CP_UTF8, 0, wTr, -1, NULL, 0, NULL, NULL);
                std::string valUtf8(len > 1 ? len - 1 : 0, 0);
                if (len > 1) WideCharToMultiByte(CP_UTF8, 0, wTr, -1, &valUtf8[0], len, NULL, NULL);
                trFile << "        <String id=\"" << g_keyNames[i] << "\">" << EscapeXml(valUtf8) << "</String>\n";
            }
            trFile << "    </Strings>\n";
            trFile << "</Language>\n";
            trFile.close();
        }
    }

    // Create en.xml if not existing
    if (GetFileAttributesW(enPath.c_str()) == INVALID_FILE_ATTRIBUTES) {
        std::ofstream enFile(enPath, std::ios::binary | std::ios::trunc);
        if (enFile.is_open()) {
            enFile << "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n";
            enFile << "<!-- Kaiseki - Professional Language Definition File -->\n";
            enFile << "<Language code=\"en\" name=\"English\" displayName=\"English\" author=\"anonimbiri\" version=\"2.5\">\n";
            enFile << "    <Strings>\n";
            for (size_t i = 0; i < (size_t)StringId::Count_; ++i) {
                // English fallback
                const wchar_t* wEn = g_builtInStrings[i][1];
                int len = WideCharToMultiByte(CP_UTF8, 0, wEn, -1, NULL, 0, NULL, NULL);
                std::string valUtf8(len > 1 ? len - 1 : 0, 0);
                if (len > 1) WideCharToMultiByte(CP_UTF8, 0, wEn, -1, &valUtf8[0], len, NULL, NULL);
                enFile << "        <String id=\"" << g_keyNames[i] << "\">" << EscapeXml(valUtf8) << "</String>\n";
            }
            enFile << "    </Strings>\n";
            enFile << "</Language>\n";
            enFile.close();
        }
    }
}

void KaisekiLang::ScanLanguages() {
    m_languages.clear();
    std::wstring langDir = GetLangDirectory();
    EnsureDefaultLanguageFiles(langDir);

    WIN32_FIND_DATAW fd;
    std::wstring searchPattern = langDir + L"\\*.xml";
    HANDLE hFind = FindFirstFileW(searchPattern.c_str(), &fd);
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                std::wstring filePath = langDir + L"\\" + fd.cFileName;
                LanguagePackInfo info;
                if (ParseLanguageHeader(filePath, info)) {
                    m_languages.push_back(info);
                }
            }
        } while (FindNextFileW(hFind, &fd));
        FindClose(hFind);
    }

    // Sort: Turkish first, English second, then alphabetical
    std::sort(m_languages.begin(), m_languages.end(), [](const LanguagePackInfo& a, const LanguagePackInfo& b) {
        if (a.code == "tr") return true;
        if (b.code == "tr") return false;
        if (a.code == "en") return true;
        if (b.code == "en") return false;
        return a.code < b.code;
    });

    // Guaranteed fallbacks
    bool hasTr = false, hasEn = false;
    for (const auto& l : m_languages) {
        if (l.code == "tr") hasTr = true;
        if (l.code == "en") hasEn = true;
    }
    if (!hasTr) {
        m_languages.insert(m_languages.begin(), { "tr", L"Türkçe", L"🇹🇷  Türkçe", L"anonimbiri", L"", true });
    }
    if (!hasEn) {
        m_languages.push_back({ "en", L"English", L"🇬🇧  English", L"anonimbiri", L"", true });
    }
}

void KaisekiLang::Initialize(const std::string& initialCode) {
    ScanLanguages();
    SetLanguage(initialCode);
}

bool KaisekiLang::SetLanguage(const std::string& code) {
    std::string lowerCode = code;
    for (char& c : lowerCode) c = (char)tolower((unsigned char)c);

    // 1. Pre-fill active strings with default built-ins (0 for Turkish, 1 for English)
    int fallbackCol = (lowerCode == "en") ? 1 : 0;
    for (size_t i = 0; i < (size_t)StringId::Count_; ++i) {
        m_activeStrings[i] = g_builtInStrings[i][fallbackCol];
    }

    // 2. Find matching language pack
    const LanguagePackInfo* foundPack = nullptr;
    for (const auto& l : m_languages) {
        if (l.code == lowerCode) {
            foundPack = &l;
            break;
        }
    }

    // 3. If found and has a valid file path, load and overlay XML
    if (foundPack && !foundPack->filePath.empty()) {
        LoadLanguageXml(foundPack->filePath, m_activeStrings);
    }

    m_currentCode = lowerCode;
    if (m_currentCode == "tr") m_langEnum = AppLanguage::Turkish;
    else if (m_currentCode == "en") m_langEnum = AppLanguage::English;
    else if (m_currentCode == "de") m_langEnum = AppLanguage::German;
    else if (m_currentCode == "ja") m_langEnum = AppLanguage::Japanese;
    else m_langEnum = AppLanguage::Custom;

    return true;
}

void KaisekiLang::SetLanguage(AppLanguage lang) {
    if (lang == AppLanguage::Turkish) SetLanguage("tr");
    else if (lang == AppLanguage::English) SetLanguage("en");
    else if (lang == AppLanguage::German) SetLanguage("de");
    else if (lang == AppLanguage::Japanese) SetLanguage("ja");
    else SetLanguage("custom");
}

const wchar_t* KaisekiLang::GetW(StringId id) const {
    size_t idx = (size_t)id;
    if (idx < m_activeStrings.size()) {
        return m_activeStrings[idx].c_str();
    }
    return L"";
}

std::string KaisekiLang::GetA(StringId id) const {
    const wchar_t* w = GetW(id);
    int len = WideCharToMultiByte(CP_UTF8, 0, w, -1, NULL, 0, NULL, NULL);
    if (len <= 1) return "";
    std::string str(len - 1, 0);
    WideCharToMultiByte(CP_UTF8, 0, w, -1, &str[0], len, NULL, NULL);
    return str;
}
