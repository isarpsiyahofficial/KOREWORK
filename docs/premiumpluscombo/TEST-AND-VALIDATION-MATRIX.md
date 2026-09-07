# Premium Plus Combo — Test ve Doğrulama Matrisi

## Sürekli korunacak kapılar

| Alan | Test | Beklenen |
|---|---|---:|
| Legacy çekirdek | `--self-test` | 180/180 |
| MOB model v4.8.25 | `--mob-model-test` | 20/20 |
| Warrior model | `--warrior-model-test` | 26/26 |
| MOB seçim model senaryoları | `mob_selector_scenario_model.py` | 48/48 |
| Gerçek Windows mouse harness | `mob_selector_manual_click_test.cpp` | 9/9 |
| v4.8.25 UI/runtime kaynak senaryoları | `v4825_mob_ui_scenario_tests.py` | 20/20 |
| PE/import | `audit_v4825_pe.py` | exact parity |
| Static core | `audit_v4825_static.py` | PASS |

## MOB seçim güvenlik senaryoları

Kontrol edilen davranışlar:
- Görsel candidate yoksa körlemesine R yok.
- Görsel candidate yanlışsa target HP/header teyidi gelmez ve candidate reddedilir.
- İlk mouse click yanlış entity'ye giderse offset/sonraki candidate denenir.
- Nameplate gövde veya oyuncu tarafından kısmen kapatıldığında görsel aday bulunamazsa Z fallback denenebilir.
- Z hedef seçmiş olsa bile target HP/header eşleşmesi olmadan confirmed olmaz.
- Confirmed flag yokken MOB chase R çalışmaz.
- Confirmed flag yokken MOB skill rotasyonu çalışmaz.
- Range fail-closed mantığı korunur.
- En fazla 7 target kaydı.

## v4.8.25 UI regresyon senaryoları

`v4825_mob_ui_scenario_tests.py` aşağıdakileri zorunlu tutar:
1. 7 HEDEF satırı kendi içinde kesişmez.
2. Son HEDEF satırı status alanından önce biter.
3. Target status save butonundan önce biter.
4. 8 SKILL satırı kendi içinde kesişmez.
5. Son SKILL satırı save butonundan önce biter.
6. SCROLL list save'den önce biter.
7. PRIEST normal sidebar style grubundadır.
8. HEDEF tabında Refresh, SKILL row açamaz.
9. Mouse scan Z fallback'ten önce çalışır.
10. Z tek başına attack authorization değildir.
11. R confirmed target ister.
12. Skills confirmed target ister.
13. Düşük parlaklıklı anti-alias kırmızı kabul edilir.
14. Nötr gri reddedilir.
15. Güçlü kırmızı kabul edilir.
16. 100% ölçek geometri çakışmaz.
17. 125% ölçek geometri çakışmaz.
18. 150% ölçek geometri çakışmaz.
19. 200% ölçek geometri çakışmaz.
20. Tüm child kontroller creation-time logical rect registry üzerinden `MoveWindow` ile ölçeklenir.

## Canlı test ile CI ayrımı

CI şu konuları garanti edemez:
- Belirli private KO client'ın tam render renkleri.
- Oyun penceresindeki gerçek frame latency.
- Client'ın aynı anda başka kullanıcı/model/efekt çizdiği anda gerçek hitbox davranışı.
- Client-specific input acceptance timing.

Bu nedenle her release sonrası kullanıcı canlı testi şu sırayla yapılmalıdır:
1. POWER + ilgili kategori aktif.
2. Görsel Tanıt ile yalnız kırmızı mob nameplate'i sıkı crop.
3. MOB HEDEF açıkken SKILL/SCROLL kontrolü görünmediğini gözle.
4. MOB hareket halindeyken doğru candidate tıklamasını izle.
5. Yanlış target geldiğinde R/skill başlamadığını doğrula.
6. Nameplate örtülünce fallback davranışını doğrula.
7. Range dışına çıkıldığında yeni target/attack başlatılmadığını doğrula.

Canlı başarısızlık otomatik testten daha yüksek öncelikli regresyon girdisidir ve bir sonraki patch raporuna kaydedilir.
