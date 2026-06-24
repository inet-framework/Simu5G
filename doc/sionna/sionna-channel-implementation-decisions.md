# Plan A — megvalósítás: döntések és összefoglaló

Ez a dokumentum a `sionna-channel-plan.md` (Plan A) Simu5G-be való megvalósítása
során feltett tervezési kérdéseket, az azokra adott válaszokat és a végeredmény
összefoglalóját rögzíti. (A parancsfuttatásra engedélyt kérő kérdések nem szerepelnek.)

Branch: `topic/bz/sionna-integration`

---

## Eldöntött kérdések és válaszok

### 1. Sionna RT függőség kezelése (fejlesztés/teszt alatt)
**Kcontextus:** a `sionna`/`sionna.rt` import a helyi Python környezetben elhasalt,
így az RT alfolyamat nem volt futtatható.
**Opciók:** (a) JSON-first + commitolt artifact stand-innel; (b) Sionna telepítése most;
(c) csak C++, Python később.
**Válasz:** **Sionna telepítése most.**
**Eredmény:** `sionna-rt 2.0.1` telepítve a venv-be (Mitsuba 3 + Dr.Jit, CPU/LLVM,
GPU/TensorFlow nélkül). A generátornak két backendje lett: determinista két-sugaras
(committed artifacthoz) **és** valódi Sionna RT — a kettő ~1 dB-en belül egyezik.

### 2. Megvalósítás hatóköre
**Opciók:** (a) teljes Plan A v1 fázisokra bontva; (b) előbb váz, majd review;
(c) csatorna CompareChannelModel nélkül.
**Válasz:** **Teljes Plan A v1, fázisokra bontva.**
**Eredmény:** minden fázis külön commit; mindegyik után debug build + fingerprint-kapu.

### 3. Mit írjon felül a `SionnaChannelModel`? (a terv §4.2 vs §9 feszültsége)
**Kontextus:** a `getAttenuation()` skalár (nincs sáv-paramétere), a `getSINR()` a
sávciklus ELŐTT vonja le, és utána antenna/kábel/szög/fading terheket ad → dupla
számolás veszélye; per-RB csak az aggregáció felülírásával lehetséges.
**Opciók:** (A) csak `getAttenuation` (wideband-only); (B) a `getSINR/getRSRP`
aggregáció felülírása; (C) szó szerint nulláról az `LteChannelModel`-ből.
**Válasz:** **B — getSINR/getRSRP aggregáció felülírása.**
**Eredmény:** a desired jel per-(link,RB) teljesítménye a táblából; az interferencia/zaj
az örökölt rutinokkal; az antenna/kábel/fading/shadowing/szög kikapcsolva (a Sionna
path gain már Tx→Rx port közötti).

### 4. Ősosztály szintje (NrChannelModel vs LteRealisticChannelModel)
**Kontextus:** a felhasználó kérte: „elemezzük az LteChannel és leszármazottjait; a cél
az, hogy a Simu5G Sionna alapján számoljon, ne csak a beépített módokon — nem a teszt
határozza meg a célt".
**Elemzés:** az `NrChannelModel::initialize()` csak az ős inicializálását hívja; az
`NrChannelModel` az `LteRealisticChannelModel`-től **kizárólag a path-loss modellben**
tér el (amit Sionna lecserél); sehol nincs `check_and_cast<NrChannelModel*>`; a teljes
aggregációs váz az `LteRealisticChannelModel`-ben lakik.
**Döntés (az elemzés alapján):** **`SionnaChannelModel : public LteRealisticChannelModel`**
— általános (LTE és NR is), nem köti NR-ízre.

### 5. Fingerprint baseline a környezeti eltérés miatt
**Kontextus:** a teljes `./fingerprints -d` 127 failure-t adott (minden `sz` és ~17
`tplx`), de 0 error — gyanú: omnet/inet verzió-eltérés a CI-baseline-hoz képest.
**Felhasználói utasítás:** mentsük el a változtatásokat, checkoutoljuk a kiinduló
állapotot, fordítsuk le, futtassuk a fingerprintet, és ha FAIL van, mentsük el a
`simulations.csv.UPDATED`-et baseline néven, tegyük be a branchbe **nyitó TMP commitnak**,
és onnantól azt használjuk a fingerprint-ellenőrzésre.
**Eredmény:** külön git worktree-ben, a **módosítatlan parent commit**-ból
(`1c5e55ff`) lefuttatva: **127 failure / 0 error**, mind a 127 sor eltér → bizonyítottan
környezeti, nem a változtatásaink. A környezet-illesztett `simulations.csv` egy
TMP commitba került („environment-matched fingerprint baseline (do not merge)").
Onnantól minden fázisnál `./fingerprints -d` → **OK**.

### 6. Sionna szcenárió a fingerprint regressziós suite-ba?
**Opciók:** (a) igen — Sionna + SionnaCompare; (b) igen — csak Sionna; (c) nem.
**Válasz:** **Igen — Sionna + SionnaCompare.**
**Eredmény:** két új sor a `simulations.csv`-ben (`/simulations/nr/sionna/`, configok
`Sionna` és `SionnaCompare`), a tényleges fingerprintek ebben a környezetben rögzítve.
Determinista (fagyasztott artifact, nincs fading-RNG) → stabil. Teljes suite: **129 teszt OK**.

### 7. A TMP környezet-baseline commit helye
**Opciók:** (a) hagyjuk a Phase 0/1 fölött; (b) átrakom a legelejére („nyitó").
**Válasz:** **Átrakom a legelejére.**
**Eredmény:** cherry-pick átrendezés egy temp branch-en, a végső fa azonosságát
ellenőrizve (`git diff` üres), a `.cproject`/`.project` felhasználói módosításokat
nem érintve. A TMP baseline most a nyitó commit, a fázisok 0–4 sorrendben követik.

---

## Megvalósítás összefoglalója

A teljes Plan A v1 implementálva, validálva, a fingerprint-suite-ba integrálva.

**Teljes fingerprint-suite: 129 teszt → OK, 0 hiba, 0 eltérés.**

### Commit-történet (`topic/bz/sionna-integration`)
```
a72cf87b test: add Sionna scenario fingerprints
595620fe Phase 4: NR example scenario + frozen channel-table artifact
ecc170f9 Phase 3: CompareChannelModel validation decorator
7a959a7c fix: resolve node phy via Binder during enumeration
551e4ff8 Phase 2: SionnaChannelModel (table-sourced propagation)
b565dfb5 Phase 1: SionnaManager global channel-table owner
30aca257 Phase 0: channel-table generator + JSON contract
683f187f TMP: environment-matched fingerprint baseline (do not merge)  ← nyitó
1c5e55ff (kiindulás)
```

### Fázisok
| Fázis | Tartalom |
|---|---|
| **0** | `sionna-rt 2.0.1` telepítve; `sionna_rt.py` generátor (determinista két-sugaras + valódi Sionna RT backend); `SCHEMA.md` (C++↔Python JSON szerződés) |
| **1** | `SionnaManager` globális modul: node-enumeráció (Binderből, phy a `getPhyByNodeId`-vel feloldva), request-hash, cache/subprocess, JSON-betöltés, pozíció-alapú lookup |
| **2** | `SionnaChannelModel : LteRealisticChannelModel`: per-(link,RB) path gain a táblából; analitikus terhek kikapcsolva |
| **3** | `CompareChannelModel : LteChannelModel`: beépített vs Sionna azonos bemeneten, delta-logolás, `primary=reference` → RNG-semleges |
| **4** | NR példa-szcenárió (`simulations/nr/sionna/`) + fagyasztott `channel_table.json` artifact |

### Új/érintett fájlok
- `src/simu5g/stack/phy/channelmodel/sionna/` — `SionnaManager.{ned,h,cc}`, `sionna_rt.py`, `SCHEMA.md`
- `src/simu5g/stack/phy/channelmodel/` — `SionnaChannelModel.{ned,h,cc}`, `CompareChannelModel.{ned,h,cc}`
- `simulations/nr/sionna/` — `SionnaSingleCell.ned`, `NrNicUeCompare.ned`, `omnetpp.ini`, `channel_table.json`, `demo.xml`, `.gitignore`
- `tests/fingerprint/simulations.csv` — +2 Sionna sor

### Validáció (futtatva)
- `Config Sionna`: végigfut VoIP-DL-lel, committed artifactból, **Sionna/Python nélkül is** (hordozható, determinista). DL SINR ~64 dB (közeli cella, interferencia nélkül).
- `Config SionnaCompare`: **Sionna vs 38.901 path-loss delta ≈ −1.15 dB** logolva (sinrDelta/rsrpDelta).
- `Config SionnaLive`: subprocess újragenerálás (tworay backend) működik.

### Fontos megjegyzések / korlátok
- A **TMP baseline commit merge előtt eldobandó** (az upstream a saját rögzített értékeit használja).
- A `CompareChannelModel` a beépített modellek shadowing/fadingjének kikapcsolását igényli: a beépített modell kereszt-modell térkép-csatolása a peer csatornamodelljét `LteRealisticChannelModel`-re castolja (a CompareChannelModel nem az). Ez egyben a terv §7 „large-scale only" fairness módja (38.901 path-loss vs Sionna átlag). Az iniben beállítva és dokumentálva.
- Az összehasonlítás a UE-oldalon (DL) történik, hogy elkerüljük a gNB-oldali háromszoros carrier-regisztrációt.
- A NR-UE két stack-azonosítóval (pl. 1025 és 2049) regisztrálódik azonos pozíción; a pozíció-alapú lookup ezt helyesen kezeli.
- **Hatókör = Plan A v1** (csatorna). Plan B (BLER-görbék, marad `PhyPisaData`), valódi geometria-scene, per-RB MIMO, Doppler/mobilitás — későbbre.
