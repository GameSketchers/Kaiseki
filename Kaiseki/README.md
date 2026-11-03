# Kaiseki — Android APK Analiz, Dekompilasyon ve Yeniden Paketleme Platformu

<p align="center">
  <img src="banner.png" alt="Kaiseki Banner" width="100%" />
</p>

<p align="center">
  <img src="https://img.shields.io/badge/S%C3%BCr%C3%BCm-v1.0.0-FF80B8?style=flat-square" alt="Sürüm" />
  <img src="https://img.shields.io/badge/Platform-Windows%2010%20%2F%2011%20(x64%20%26%20x86)-0078D6?style=flat-square&logo=windows" alt="Platform" />
  <img src="https://img.shields.io/badge/Motor-Yerel%20C%2B%2B20-blue?style=flat-square&logo=c%2B%2B" alt="C++20" />
  <img src="https://img.shields.io/badge/Grafik-Direct2D%20%2F%20DirectWrite-A55EEA?style=flat-square" alt="Direct2D" />
  <img src="https://img.shields.io/badge/Lisans-GPL--3.0-green?style=flat-square" alt="GPLv3" />
</p>

---

## Genel Bakış

**Kaiseki**, Android APK paketlerini derinlemesine incelemek, Dalvik DEX bayt kodunu ayrıştırmak, kaynakları ve AndroidManifest.xml dosyasını çözümlemek, Smali ve Java benzeri sözdizimi üzerinde değişiklik yapmak, APK'yı yeniden derlemek, hizalamak (zipalign), v1/v2/v3 şemalarıyla imzalamak ve ADB üzerinden Android cihazlara doğrudan yüklemek üzere geliştirilmiş yerel (native) Windows masaüstü platformudur.

Harici JVM (Java Sanal Makinesi) çalıştırma zorunluluğu olmadan çalışan Kaiseki, doğrudan C++20 ve donanım hızlandırmalı Direct2D/DirectWrite grafik altyapısı üzerine inşa edilmiştir.

---

## Temel Özellikler

### 1. APK Ayrıştırma, Çözümleme ve Paket Ağacı
- **Doğrudan PKZIP / APK Okuma:** APK paketlerini bellekte veya geçici çalışma dizininde hızla açar, dizin ve dosya hiyerarşisini listeler.
- **İkili XML (AXML) Çözücü:** `AndroidManifest.xml` ve derlenmiş layout/anim XML dosyalarını okunabilir metin formatına dönüştürür.
- **DEX ve Multi-DEX Desteği:** `classes.dex`, `classes2.dex` ... `classesN.dex` dosyalarını tarar; sınıf, metot, alan ve dize referanslarını indeksler.
- **Paket Hiyerarşisi Gezgini:** Kaynak kodları ve kaynakları (`res/`, `assets/`, `lib/`, `smali/`) iç içe açılır-kapanır hiyerarşik klasör yapısında sunar.

### 2. Kaynak Kodu ve Smali Editörü
- **Doğrudan Dalvik Ayrıştırma:** Smali bayt kodunu ve yapısal Java benzeri soyut sözdizimini (AST) görüntüler.
- **Çift Yönlü Görünüm (Smali ⇄ Java):** Tek tıklamayla veya kısayolla Smali ve Java görünümleri arasında geçiş sağlar.
- **DirectWrite Metin Editörü:** Donanım hızlandırmalı sub-pixel yazı tipi işleme, satır numaralandırması, sözdizimi renklendirmesi ve otomatik gizlenen kaydırma çubukları.
- **Düzenleme ve Kaydetme:** Açık sekmelerde yapılan değişiklikleri kaydeder (`Ctrl+S`, `Ctrl+Shift+S`) ve geri alma/yineleme geçmişini yönetir.

### 3. Güvenlik ve Zafiyet Analiz Motoru (`KaisekiSec`)
- **Güvenlik Puanlaması:** APK bileşenlerini güvenlik standartlarına göre tarayarak 0-100 arasında güvenlik skoru hesaplar.
- **Kritik/Yüksek Risk Tespiti:** Dışa aktarılmış (exported) güvensiz bileşenler, açık metin (cleartext) HTTP trafiği, debuggable bayrağı, zayıf kriptografik algoritmalar ve sabit kodlanmış hassas dizeleri listeler.
- **Frida ve Güvenlik Raporu:** Uygulama için evrensel Frida güvenlik atlatma (bypass) komut dosyası ve ayrıntılı güvenlik denetim raporu üretir.

### 4. Yeniden Paketleme, Zipalign ve İmzalama (`KaisekiCore`)
- **APK Yeniden Derleme:** Değiştirilen dosya ve bayt kodlarını yeni bir APK paketi olarak derler.
- **4-Byte Zipalign:** Sıkıştırılmamış verileri 4-byte sınırlarına hizalayarak Android bellek performansını optimize eder.
- **İmza Şemaları (v1, v2 ve v3):** Resmi Android SDK `apksigner` veya dahili PKCS#7 / APK Signing Block motoruyla JAR (v1), APK Signing Block (v2) ve Android 9+ (v3) şemalarında dijital imzalama yapar.

### 5. Android Cihaz Köprüsü (ADB) ve Canlı Logcat (`KaisekiADB`)
- **Cihaz Yönetimi:** USB veya Wi-Fi üzerinden bağlı Android telefon, tablet ve emülatörleri otomatik algılar.
- **Tek Tuşla Yükleme (F10):** İmzalanmış APK'yı doğrudan seçili cihaza yükler ve başlatır.
- **Canlı Logcat Terminali:** Seçili uygulamanın log çıktılarını filtreler, hata ve uyarı seviyelerine göre renklendirir.

### 6. Hex Görüntüleyici ve Entropi Analizi (`KaisekiHex`)
- **Bayt Seviyesinde İnceleme:** ELF paylaşımlı kütüphaneleri (`.so`), DEX ve ham varlıkları onaltılık (hex) formatta inceler.
- **Shannon Entropisi:** Gizlenmiş, şifrelenmiş veya paketlenmiş bölümleri entropi haritasıyla tespit eder.

### 7. APK Karşılaştırma ve Fark Analizi (`KaisekiDiff`)
- İki APK paketi arasındaki dosya boyutu, sınıf sayısı, izinler, eklenen ve kaldırılan sınıflar ile bayt kodu farklarını yan yana karşılaştırır.

### 8. Modal Ayarlar ve Tercihler Penceresi
- Ana pencereden bağımsız, modal Windows iletişim kutusu olarak açılır (`HWND`).
- **Görünüm & Tema:** 10 vurgu rengi ve 4 koyu arka plan tonu ile anında HSV 2D renk seçimi.
- **Dil Seçimi:** Türkçe, İngilizce, Almanca ve Japonca dil paketleri arasında anında geçiş.
- **Android SDK ve Araç Yöneticisi:** `platform-tools`, `build-tools` ve `cmdline-tools` yollarını denetleme ve eksik araçları indirme paneli.
- **Kısayol ve Dekompilasyon Tercihleri:** Hızlı geçiş anahtarları (toggle switch) ve klavye düzeni ayarları.

---

## Modüler Mimari

Kaiseki, bağımsız DLL modüllerinden oluşan genişletilebilir bir mimariye sahiptir:

| Modül | Dosya Adı | Açıklama |
|---|---|---|
| **Ana Uygulama** | `Kaiseki.exe` | Ana kabuk, komut satırı parametreleri ve tek örnek (single-instance) denetimi |
| **Arayüz Motoru** | `KaisekiUI.dll` | Direct2D/DirectWrite grafik motoru, editör, gezgin ve iletişim pencereleri |
| **Çekirdek Motor** | `KaisekiCore.dll` | APK açma, ZIP akışları, paketleme, zipalign ve imzalama |
| **Dalvik Motoru** | `KaisekiDex.dll` | DEX bayt kodu çözümleyici, Smali ayrıştırıcı ve sözdizimi üretici |
| **ADB Motoru** | `KaisekiADB.dll` | Android Debug Bridge entegrasyonu, cihaz iletişimi ve Logcat akışı |
| **Güvenlik Motoru** | `KaisekiSec.dll` | Zafiyet tarayıcı, risk analizi ve sertifika doğrulama |
| **Hex & Entropi** | `KaisekiHex.dll` | Onaltılık bayt görüntüleme ve kriptografik entropi haritası |
| **Kaynak Motoru** | `KaisekiRes.dll` | İkili XML (`AxmlEngine`) ve `resources.arsc` derleme/çözümleme |
| **Fark Motoru** | `KaisekiDiff.dll` | İki APK arasındaki sınıf ve bayt kodu fark hesaplaması |
| **Animasyon Motoru**| `KaisekiAnim.dll` | DWM pencere geçişleri, yumuşatma eğrileri ve yay dinamikleri |

---

## Kısayollar

| Kısayol | İşlev |
|---|---|
| `Ctrl+O` | APK veya Proje Dosyası Aç |
| `Ctrl+S` | Etkin Belge Sekmesini Kaydet |
| `Ctrl+Shift+S` | Değiştirilen Tüm Belgeleri Kaydet |
| `Ctrl+Z` / `Ctrl+Y` | Geri Al / Yinele |
| `Ctrl+F` | Etkin Belgede Bul |
| `Ctrl+Shift+F` | Tüm Projede Genel Arama |
| `Ctrl+W` | Etkin Belgeyi Kapat |
| `F5` | Proje Gezginini ve Logcat'i Yenile |
| `F7` | Dalvik Bayt Kodunu Dekompile Et |
| `F8` | Değişiklikleri Paketle ve İmzala |
| `F10` | Cihaza Yükle (ADB Deploy) |
| `Ctrl+,` | Ayarlar ve Tercihler Penceresini Aç |

---

## Kaynaktan Derleme

### Gereksinimler
- Windows 10 veya Windows 11 (64-bit veya 32-bit)
- Microsoft Visual Studio 2022 (MSVC C++20 standardı)
- Windows SDK (10.0.22000.0 veya üzeri)

### Derleme Adımları
Kök dizindeki toplu işlem komut dosyasını çalıştırarak hem x64 hem de x86 mimarilerini tek seferde derleyebilirsiniz:

```cmd
build.bat all
```

Sadece belirli bir mimariyi derlemek için:
```cmd
build.bat x64
:: veya
build.bat x86
```

Derleme çıktıları `bin\x64\` ve `bin\x86\` dizinlerinde oluşturulur.

---

## Lisans

Bu proje **GNU General Public License v3.0 (GPLv3)** ile lisanslanmıştır. Projenin kaynak kodları her zaman açık kalmak zorundadır ve türetilen çalışmalar da aynı lisans koşullarıyla dağıtılmalıdır. Ayrıntılar için [LICENSE](LICENSE) dosyasına bakınız.
