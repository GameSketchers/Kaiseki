#define _CRT_SECURE_NO_WARNINGS
#include "KaisekiDiffApi.h"
#include <sstream>
#include <map>
#include <set>
#include <algorithm>
#include <iomanip>

#include "KaisekiVersion.h"

extern "C" {

const char* KaisekiDiff_GetVersion() {
#if defined(_DEBUG)
    return KAISEKI_VERSION_STRING "-debug-v143-mdd";
#else
    return KAISEKI_VERSION_STRING "-release-v143-md";
#endif
}

void KaisekiDiff_FreeSummary(ApkDiffSummary* summary) {
    if (!summary) return;
    std::vector<std::string>().swap(summary->permsAdded);
    std::vector<std::string>().swap(summary->permsRemoved);
    std::vector<std::string>().swap(summary->activitiesAdded);
    std::vector<std::string>().swap(summary->activitiesRemoved);
    std::vector<std::string>().swap(summary->servicesAdded);
    std::vector<std::string>().swap(summary->servicesRemoved);
    std::vector<DiffClassEntry>().swap(summary->changedClasses);
    std::vector<DiffFileEntry>().swap(summary->fileDiffs);
    std::string().swap(summary->formattedReport);
}

void KaisekiDiff_FreeDiffString(std::string* diff) {
    if (diff) std::string().swap(*diff);
}

bool KaisekiDiff_GenerateUnifiedDiff(const char* textA, const char* textB, std::string* outDiff) {
    if (!outDiff) return false;
    outDiff->clear();

    std::vector<std::string> linesA;
    std::vector<std::string> linesB;

    if (textA) {
        std::istringstream sa(textA);
        std::string line;
        while (std::getline(sa, line)) linesA.push_back(line);
    }
    if (textB) {
        std::istringstream sb(textB);
        std::string line;
        while (std::getline(sb, line)) linesB.push_back(line);
    }

    std::ostringstream oss;
    oss << "--- Orijinal (A)\n";
    oss << "+++ Karşılaştırılan (B)\n";
    oss << "@@ -1," << linesA.size() << " +1," << linesB.size() << " @@\n";

    size_t maxLen = (std::max)(linesA.size(), linesB.size());
    for (size_t i = 0; i < maxLen; ++i) {
        if (i < linesA.size() && i < linesB.size()) {
            if (linesA[i] == linesB[i]) {
                oss << "  " << linesA[i] << "\n";
            } else {
                oss << "- " << linesA[i] << "\n";
                oss << "+ " << linesB[i] << "\n";
            }
        } else if (i < linesA.size()) {
            oss << "- " << linesA[i] << "\n";
        } else if (i < linesB.size()) {
            oss << "+ " << linesB[i] << "\n";
        }
    }

    *outDiff = oss.str();
    return true;
}

bool KaisekiDiff_CompareApks(const wchar_t* apkPathA, const wchar_t* apkPathB, ApkDiffSummary* outSummary) {
    if (!apkPathA || !apkPathB || !outSummary) return false;
    if (outSummary->structSize != sizeof(ApkDiffSummary)) return false;

    KaisekiDiff_FreeSummary(outSummary);

    ApkDataModel* modelA = nullptr;
    ApkDataModel* modelB = nullptr;

    KaisekiResult resA = ApkCore_InspectFile(apkPathA, &modelA);
    KaisekiResult resB = ApkCore_InspectFile(apkPathB, &modelB);

    if (!resA || !modelA || !resB || !modelB) {
        if (modelA) ApkCore_FreeModel(modelA);
        if (modelB) ApkCore_FreeModel(modelB);
        return false;
    }

    outSummary->pathA = apkPathA;
    outSummary->pathB = apkPathB;
    outSummary->nameA = modelA->fileName;
    outSummary->nameB = modelB->fileName;
    outSummary->sizeA = modelA->fileSize;
    outSummary->sizeB = modelB->fileSize;

    outSummary->versionA = modelA->versionName + " (" + modelA->versionCode + ")";
    outSummary->versionB = modelB->versionName + " (" + modelB->versionCode + ")";
    outSummary->sdkA = "Min " + modelA->minSdk + " / Target " + modelA->targetSdk;
    outSummary->sdkB = "Min " + modelB->minSdk + " / Target " + modelB->targetSdk;

    // 1. Permissions Diff
    std::set<std::string> permsA(modelA->permissions.begin(), modelA->permissions.end());
    std::set<std::string> permsB(modelB->permissions.begin(), modelB->permissions.end());

    for (const auto& p : permsB) {
        if (permsA.find(p) == permsA.end()) {
            outSummary->permsAdded.push_back(p);
        }
    }
    for (const auto& p : permsA) {
        if (permsB.find(p) == permsB.end()) {
            outSummary->permsRemoved.push_back(p);
        }
    }

    // 2. Activities & Services Diff
    std::set<std::string> actA(modelA->activities.begin(), modelA->activities.end());
    std::set<std::string> actB(modelB->activities.begin(), modelB->activities.end());
    for (const auto& a : actB) { if (actA.find(a) == actA.end()) outSummary->activitiesAdded.push_back(a); }
    for (const auto& a : actA) { if (actB.find(a) == actB.end()) outSummary->activitiesRemoved.push_back(a); }

    std::set<std::string> srvA(modelA->services.begin(), modelA->services.end());
    std::set<std::string> srvB(modelB->services.begin(), modelB->services.end());
    for (const auto& s : srvB) { if (srvA.find(s) == srvA.end()) outSummary->servicesAdded.push_back(s); }
    for (const auto& s : srvA) { if (srvB.find(s) == srvB.end()) outSummary->servicesRemoved.push_back(s); }

    // 3. File & Resource Comparison
    std::map<std::string, uint32_t> filesA;
    std::map<std::string, uint32_t> filesB;
    for (const auto& r : modelA->resources) filesA[r.path] = r.uncompressedSize;
    for (const auto& r : modelB->resources) filesB[r.path] = r.uncompressedSize;

    outSummary->totalFilesA = (int)filesA.size();
    outSummary->totalFilesB = (int)filesB.size();

    for (const auto& kv : filesB) {
        auto it = filesA.find(kv.first);
        if (it == filesA.end()) {
            outSummary->filesAdded++;
            outSummary->fileDiffs.push_back({ kv.first, DiffItemStatus::Added, 0, kv.second });
        } else if (it->second != kv.second) {
            outSummary->filesModified++;
            outSummary->fileDiffs.push_back({ kv.first, DiffItemStatus::Modified, it->second, kv.second });
        }
    }
    for (const auto& kv : filesA) {
        if (filesB.find(kv.first) == filesB.end()) {
            outSummary->filesRemoved++;
            outSummary->fileDiffs.push_back({ kv.first, DiffItemStatus::Removed, kv.second, 0 });
        }
    }

    // 4. DEX Class Comparison
    std::map<std::string, int> classesA;
    std::map<std::string, int> classesB;
    for (const auto& pkg : modelA->packages) {
        for (const auto& cls : pkg.classes) classesA[cls.fullClassName] = cls.methodCount;
    }
    for (const auto& pkg : modelB->packages) {
        for (const auto& cls : pkg.classes) classesB[cls.fullClassName] = cls.methodCount;
    }

    for (const auto& kv : classesB) {
        auto it = classesA.find(kv.first);
        if (it == classesA.end()) {
            outSummary->classesAdded++;
            outSummary->changedClasses.push_back({ kv.first, DiffItemStatus::Added, 0, kv.second });
        } else if (it->second != kv.second) {
            outSummary->classesModified++;
            outSummary->changedClasses.push_back({ kv.first, DiffItemStatus::Modified, it->second, kv.second });
        }
    }
    for (const auto& kv : classesA) {
        if (classesB.find(kv.first) == classesB.end()) {
            outSummary->classesRemoved++;
            outSummary->changedClasses.push_back({ kv.first, DiffItemStatus::Removed, kv.second, 0 });
        }
    }

    // 5. Signature Tampering Evaluation
    outSummary->signerA = modelA->signature.signerSubject;
    outSummary->signerB = modelB->signature.signerSubject;
    outSummary->sha256A = modelA->signature.certSha256;
    outSummary->sha256B = modelB->signature.certSha256;

    outSummary->tamperingScore = 0;
    if (!outSummary->sha256A.empty() && !outSummary->sha256B.empty() &&
        outSummary->sha256A != outSummary->sha256B) {
        outSummary->certMatches = false;
        outSummary->tamperingScore += 65; // Keystore mismatch is primary indicator of repackaging/cracking!
    } else {
        outSummary->certMatches = true;
    }

    // Evaluate newly added dangerous permissions
    for (const auto& p : outSummary->permsAdded) {
        std::string pUp = p;
        for (char& c : pUp) c = (char)toupper((unsigned char)c);
        if (pUp.find("SEND_SMS") != std::string::npos ||
            pUp.find("SYSTEM_ALERT_WINDOW") != std::string::npos ||
            pUp.find("RECORD_AUDIO") != std::string::npos ||
            pUp.find("CAMERA") != std::string::npos) {
            outSummary->tamperingScore += 20;
        }
    }

    if (outSummary->tamperingScore > 100) outSummary->tamperingScore = 100;

    if (outSummary->tamperingScore >= 60) {
        outSummary->tamperingVerdict = "🚨 KRİTİK GÜVENLİK RİSKİ (Farklı Sertifika ile Yeniden Paketlenmiş / Modlu APK)";
    } else if (outSummary->tamperingScore >= 25) {
        outSummary->tamperingVerdict = "⚠️ DİKKAT (Yeni Kritik İzinler ve Kod Değişiklikleri Tespit Edildi)";
    } else if (outSummary->filesModified > 0 || outSummary->filesAdded > 0 || outSummary->classesModified > 0) {
        outSummary->tamperingVerdict = "✅ STANDART SÜRÜM FARKLILIĞI (Resmi Geliştirici Güncellemesi)";
    } else {
        outSummary->tamperingVerdict = "✅ ÖZDEŞ (İki APK Arasında Anlamsal Fark Bulunamadı)";
    }

    // 6. Generate Comprehensive Formatted Report
    std::ostringstream ss;
    ss << "================================================================================\n";
    ss << " KAISEKI STUDIO - İKİ APK KARŞILAŞTIRMA VE FARK RAPORU (KaisekiDiff Engine)\n";
    ss << "================================================================================\n";
    ss << "Hedef A (Orijinal):       " << modelA->packageName << " v" << outSummary->versionA << "\n";
    ss << "Hedef B (Karşılaştırılan): " << modelB->packageName << " v" << outSummary->versionB << "\n";
    ss << "Dosya Boyutu A:           " << (outSummary->sizeA / 1024) << " KB\n";
    ss << "Dosya Boyutu B:           " << (outSummary->sizeB / 1024) << " KB ("
       << ((int64_t)outSummary->sizeB - (int64_t)outSummary->sizeA > 0 ? "+" : "")
       << ((int64_t)outSummary->sizeB - (int64_t)outSummary->sizeA) / 1024 << " KB)\n";
    ss << "--------------------------------------------------------------------------------\n";
    ss << "DURUM VE GÜVENLİK DEĞERLENDİRMESİ:\n";
    ss << "  Manipülasyon Riski Skoru: " << outSummary->tamperingScore << " / 100\n";
    ss << "  Sonuç Bildirimi:          " << outSummary->tamperingVerdict << "\n";
    ss << "  İmza Sertifikası Uyumu:   " << (outSummary->certMatches ? "BİREBİR AYNI (Güvenli ✅)" : "UYUŞMUYOR (Farklı Keystore ile İmzalanmış ❌)") << "\n";
    if (!outSummary->certMatches) {
        ss << "    [!] Sertifika A SHA-256: " << outSummary->sha256A << "\n";
        ss << "    [!] Sertifika B SHA-256: " << outSummary->sha256B << "\n";
    }
    ss << "--------------------------------------------------------------------------------\n";
    ss << "DOSYA VE İÇERİK METRİKLERİ:\n";
    ss << "  Toplam Dosya A: " << outSummary->totalFilesA << "  |  Toplam Dosya B: " << outSummary->totalFilesB << "\n";
    ss << "  Eklenen Dosyalar: " << outSummary->filesAdded << "\n";
    ss << "  Silinen Dosyalar:  " << outSummary->filesRemoved << "\n";
    ss << "  Değişen Dosyalar:  " << outSummary->filesModified << "\n";
    ss << "--------------------------------------------------------------------------------\n";
    ss << "DEX BAYTKOD METRİKLERİ:\n";
    ss << "  Toplam Sınıf A: " << modelA->totalClasses << "  |  Toplam Sınıf B: " << modelB->totalClasses << "\n";
    ss << "  Eklenen Sınıflar:    " << outSummary->classesAdded << "\n";
    ss << "  Silinen Sınıflar:     " << outSummary->classesRemoved << "\n";
    ss << "  Değiştirilen Sınıflar:" << outSummary->classesModified << "\n";
    ss << "--------------------------------------------------------------------------------\n";

    if (!outSummary->permsAdded.empty()) {
        ss << "\n[+] YENİ EKLENEN İZİNLER (APK B'de Mevcut, APK A'da Yok):\n";
        for (const auto& p : outSummary->permsAdded) {
            ss << "  + " << p << "\n";
        }
    }
    if (!outSummary->permsRemoved.empty()) {
        ss << "\n[-] KALDIRILAN İZİNLER (APK A'da Vardı, APK B'de Çıkarılmış):\n";
        for (const auto& p : outSummary->permsRemoved) {
            ss << "  - " << p << "\n";
        }
    }

    if (!outSummary->activitiesAdded.empty()) {
        ss << "\n[+] YENİ EKLENEN AKTİVİTELER (Activities):\n";
        for (const auto& a : outSummary->activitiesAdded) ss << "  + " << a << "\n";
    }

    if (!outSummary->changedClasses.empty()) {
        ss << "\n[*] DEĞİŞTİRİLEN VE EKLENEN KRİTİK SINIFLAR (İlk 25 Örnek):\n";
        size_t count = 0;
        for (const auto& c : outSummary->changedClasses) {
            if (++count > 25) { ss << "  ... ve " << (outSummary->changedClasses.size() - 25) << " sınıf daha.\n"; break; }
            std::string st = (c.status == DiffItemStatus::Added) ? "[EKLENDİ]" :
                             ((c.status == DiffItemStatus::Modified) ? "[DEĞİŞTİ]" : "[SİLİNDİ]");
            ss << "  " << st << " " << c.className << " (Metot Sayısı: " << c.methodsA << " -> " << c.methodsB << ")\n";
        }
    }

    if (!outSummary->fileDiffs.empty()) {
        ss << "\n[*] DOSYA SEVİYESİ FARKLILIKLAR (İlk 30 Örnek):\n";
        size_t count = 0;
        for (const auto& f : outSummary->fileDiffs) {
            if (++count > 30) { ss << "  ... ve " << (outSummary->fileDiffs.size() - 30) << " dosya daha.\n"; break; }
            std::string st = (f.status == DiffItemStatus::Added) ? "[+] EKLENDİ   " :
                             ((f.status == DiffItemStatus::Modified) ? "[*] DEĞİŞTİRİLDİ" : "[-] SİLİNDİ    ");
            ss << "  " << st << " " << f.path << " (" << f.sizeA << " bayt -> " << f.sizeB << " bayt)\n";
        }
    }

    ss << "\n================================================================================\n";
    ss << " [KaisekiDiff] Analiz başarıyla tamamlandı.\n";

    outSummary->formattedReport = ss.str();

    ApkCore_FreeModel(modelA);
    ApkCore_FreeModel(modelB);
    return true;
}

}
