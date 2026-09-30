# Attestation trust store (`paa_cert/`)

Every production PAA root certificate approved on the CSA DCL MainNet, fetched on 2026-09-30 with
`connectedhomeip/credentials/fetch_paa_certs_from_dcl.py --use-main-net-http`.
Files are renamed because SPIFFS allows only 32-character paths. MYGGBETT chains to `ikea_g1.der`.
Matter's test PAAs are deliberately left out.

| File | Subject | SHA-256 (first 16 hex) |
|---|---|---|
| `ikea_g1.der` | 1.3.6.1.4.1.37244.2.1=#0C0431313743,CN=IKEA of Sweden Matter PAA G1 | `c082419ec362c074` |
| `p00.der` | 1.3.6.1.4.1.37244.2.1=#0C0431343042,CN=70mai Matter PAA | `128157979ea0637e` |
| `p01.der` | 1.3.6.1.4.1.37244.2.1=#0C0431333741,CN=ACK PAA | `a37a4bb829d1aa7b` |
| `p02.der` | CN=Anker Innovations Matter PAA,1.3.6.1.4.1.37244.2.1=#0C0431353333 | `30c2978d1df9b50c` |
| `p03.der` | CN=Aqara Matter PAA #01,O=Lumi United Technology Co.\, Ltd | `8ab1dc34a8dfd918` |
| `p04.der` | 1.3.6.1.4.1.37244.2.1=#0C0431333742,CN=Basics PAA | `678f8946471fae37` |
| `p05.der` | 1.3.6.1.4.1.37244.2.1=#0C0431333044,CN=BouffaloLab Matter PAA | `e1be4c84817e939f` |
| `p06.der` | CN=CableLabs Matter PAA | `984ce03b57124db4` |
| `p07.der` | CN=CommScope Technologies LLC Matter PAA | `07de366a41fe5171` |
| `p08.der` | 1.3.6.1.4.1.37244.2.1=#0C0431323836,CN=Coolkit Matter PAA | `9f54dc2ee25d94ee` |
| `p09.der` | CN=Cybertrust Matter PAA G1 | `64a46acd42bf4cd2` |
| `p10.der` | CN=DSC Matter PAA,O=Dream Security Co.\, Ltd.,C=KR | `aa8931be961dcf66` |
| `p11.der` | CN=DigiCert Root CA for MATTER PKI G1,O=DigiCert\, Inc.,C=US | `6fff1847f32de014` |
| `p12.der` | 1.3.6.1.4.1.37244.2.1=#0C0431333635,CN=Dooya Matter PAA | `24211dcfe05ce0c4` |
| `p13.der` | O=ECOVACS IOT,1.3.6.1.4.1.37244.2.1=#0C0431343035,CN=Ecovacs Matter PAA CN | `ecccd0f57e0154f0` |
| `p14.der` | O=ECOVACS IOT,1.3.6.1.4.1.37244.2.1=#0C0431343035,CN=Ecovacs Matter PAA | `6dabaf098b18c309` |
| `p15.der` | 1.3.6.1.4.1.37244.2.1=#0C0431343632,CN=Energy Magic Cube Matter PAA 001 | `91be7dc279bd858b` |
| `p16.der` | O=Espressif Systems,CN=Espressif Matter Open PAA | `e849377a5626fe63` |
| `p17.der` | 1.3.6.1.4.1.37244.2.1=#0C0431333142,O=Espressif Systems,CN=Espressif Matter PAA | `f33cc64aaae8480d` |
| `p18.der` | 1.3.6.1.4.1.37244.2.1=#0C0431343233,CN=Feit Electric PAA | `c0304b6789969060` |
| `p19.der` | 1.3.6.1.4.1.37244.2.1=#0C0431343131,CN=Freedompro\,vid=0x1411 | `fb0964c3ebe7a64c` |
| `p20.der` | 1.3.6.1.4.1.37244.2.1=#0C0431343730,CN=HOPERF Matter PAA 01 | `c0a7a26d07f4dc3f` |
| `p21.der` | 1.3.6.1.4.1.37244.2.1=#0C0431333531,CN=HooRii Matter PAA G1 | `c74081ba2ad4752c` |
| `p22.der` | 1.3.6.1.4.1.37244.2.1=#0C0431353137,CN=HuaCheng | `ff2613017f65b177` |
| `p23.der` | 1.3.6.1.4.1.37244.2.1=#0C0431333931,CN=Kasa Matter PAA | `b64c2f8b8f196514` |
| `p24.der` | CN=Kudelski Matter PAA 01 | `ed64dcfff0cd6f77` |
| `p25.der` | 1.3.6.1.4.1.37244.2.1=#0C0431343231,CN=Kwikset Matter PAA,O=Kwikset | `da6fb7ca852dfce7` |
| `p26.der` | 1.3.6.1.4.1.37244.2.1=#0C0431313638,CN=LEEDARSON-MATTER-PAA | `dcc110546e9a1d64` |
| `p27.der` | 1.3.6.1.4.1.37244.2.1=#0C0431303231,CN=Legrand Group Matter PAA | `7e0d9db1806b8a49` |
| `p28.der` | 1.3.6.1.4.1.37244.2.1=#0C0431303942,CN=Leviton PAA | `8c08d66b6f5c1baf` |
| `p29.der` | 1.3.6.1.4.1.37244.2.1=#0C0431333146,CN=Longan.link Matter PAA 01 | `98c54ce572d83d7a` |
| `p30.der` | 1.3.6.1.4.1.37244.2.1=#0C0431304632,CN=Matter PAA #1,O=ubisys technologies GmbH | `f3c71e43d60b2781` |
| `p31.der` | 1.3.6.1.4.1.37244.2.1=#0C0436303036,CN=Matter PAA 2,O=Google,C=US | `565ebefde9f4c57d` |
| `p32.der` | 1.3.6.1.4.1.37244.2.1=#0C0431303042,CN=Matter Signify PAA 1 | `69b83b96a36ec9bd` |
| `p33.der` | 1.3.6.1.4.1.37244.2.1=#0C0431343030,CN=Matter Uascent PAA 0x1400 | `2b05282a57a7b716` |
| `p34.der` | 1.3.6.1.4.1.37244.2.1=#0C0431333435,CN=Meross Matter PAA | `f5e7a43aac8ba450` |
| `p35.der` | 1.3.6.1.4.1.37244.2.1=#130431313843,CN=Midea Group Matter PAA G1 Prod | `d5c253fe2b930a5a` |
| `p36.der` | CN=Nexus Matter PAA G1,O=Technology Nexus SBS AB,C=SE | `0e0e1af671573b63` |
| `p37.der` | O=PanKore,1.3.6.1.4.1.37244.2.1=#0C0431333136,CN=PanKorePAA | `125a16bdbd040387` |
| `p38.der` | 1.3.6.1.4.1.37244.2.1=#0C0431343034,CN=Phaten Matter PAA | `571c4eb38b0283f0` |
| `p39.der` | 1.3.6.1.4.1.37244.2.1=#0C0431343044,CN=PhotonSail Matter PAA | `7de36098faaff5cf` |
| `p40.der` | 1.3.6.1.4.1.37244.2.1=#0C0431333831,CN=Prime PAA | `b5f1b547e5a50cb8` |
| `p41.der` | 1.3.6.1.4.1.37244.2.1=#0C0431333837,CN=Qianyan Matter PAA | `c7a229cb0aee2c4c` |
| `p42.der` | 1.3.6.1.4.1.37244.2.1=#0C0431343130,CN=Quectel Matter PAA | `df0e30c5afbdf062` |
| `p43.der` | 1.3.6.1.4.1.37244.2.1=#0C0431333436,CN=Rafael Matter PAA | `40988ee2ad03023f` |
| `p44.der` | 1.3.6.1.4.1.37244.2.1=#0C0431333141,CN=Resideo Matter PAA | `c7cb82a0663000b6` |
| `p45.der` | C=US,O=SSL Corporation,CN=SSL.com Matter ECC Root CA 2025 | `c69bd04172fc526a` |
| `p46.der` | CN=STMicroelectronics Matter PAA 01 | `6ae8cd997aafa828` |
| `p47.der` | 1.3.6.1.4.1.37244.2.1=#0C0431353036,CN=Safemo Matter PAA | `63f146f672da78e8` |
| `p48.der` | 1.3.6.1.4.1.37244.2.1=#0C0431303545,CN=Schneider Electric Matter PAA 01 | `caf43178235aa38c` |
| `p49.der` | 1.3.6.1.4.1.37244.2.1=#0C0431313630,CN=Sengled Matter PAA | `e6cc9002ac515281` |
| `p50.der` | 1.3.6.1.4.1.37244.2.1=#0C0431313331,CN=Sercomm-Matter-PAA-01 | `9929fc35ac64ddcc` |
| `p51.der` | 1.3.6.1.4.1.37244.2.1=#0C0431323830,CN=Siterwell Matter PAA | `a99892385127fc02` |
| `p52.der` | CN=Snowball Matter PAA 01 | `29810893f7d1936a` |
| `p53.der` | O=StrongKey,CN=StrongKey Matter G1 PAA | `11dcd79646976752` |
| `p54.der` | 1.3.6.1.4.1.37244.2.1=#0C0431333937,CN=SwitchBot Matter PAA | `ee56260639d79b0a` |
| `p55.der` | 1.3.6.1.4.1.37244.2.1=#0C0431313838,CN=TP-Link Matter PAA | `0d5d74ec2a4cbc5c` |
| `p56.der` | 1.3.6.1.4.1.37244.2.1=#0C0431333932,CN=Tapo Matter PAA | `f2edb04e538881c0` |
| `p57.der` | CN=TrustAsia Matter PAA,O=TrustAsia Technologies\, Inc. | `40261fc989e56900` |
| `p58.der` | CN=Tuya Global Matter PAA | `85fc4df60179f920` |
| `p59.der` | 1.3.6.1.4.1.37244.2.1=#0C0431323544,CN=Tuya Matter PAA | `766ab851f8780265` |
| `p60.der` | 1.3.6.1.4.1.37244.2.1=#0C0431343746,CN=U-tec Group Matter PAA | `31272a8a3420ccb7` |
| `p61.der` | 1.3.6.1.4.1.37244.2.1=#0C0431304545,CN=UEI Development PAA | `08d9571203876c5f` |
| `p62.der` | CN=WISeKey OISTE Matter PAA GA | `49aceccb14918383` |
| `p63.der` | 1.3.6.1.4.1.37244.2.1=#0C0431313144,CN=XFN Matter PAA VID | `10e1bd1fd99035d7` |
| `p64.der` | 1.3.6.1.4.1.37244.2.1=#0C0431323645,CN=Xiaomi Mijia Matter PAA | `0ccc5b6f7ba4fcac` |
| `p65.der` | 1.3.6.1.4.1.37244.2.1=#0C0431333132,CN=Yeelight Matter PAA | `ef08581b122e81fa` |
| `p66.der` | 1.3.6.1.4.1.37244.2.1=#0C0431323042,CN=heiman Matter Protocol PAA | `6c2c2a1f531db7f0` |
| `p67.der` | 1.3.6.1.4.1.37244.2.1=#0C0431353136,CN=iRobot Main-Net PAA | `a5328d69708bb32d` |
| `p68.der` | O=PuzL Labs LLC,1.3.6.1.4.1.37244.2.1=#0C0431343944,CN=paa-prod.puzllabs.com | `0398fe3e32f9f494` |
| `p69.der` | CN=NXP Matter PAA,C=NL,O=NXP Semiconductors N.V.,serialNumber=63709330400001 | `8a6e4a8f2edd76d7` |
| `p70.der` | serialNumber=63709330400004,CN=NXP Matter PAA G2 | `ad87117d621b35b5` |
