# Code review — `topic/bz/sionna-integration` (Sionna Plan A)

Magas-effort review (8 finder-szög + verifikáció) a Sionna-munka diffjén
(`1c5e55ff..HEAD`), a `simulations.csv` környezeti tömeg-rögzítését és a doc-okat
kihagyva. A jelölteket dedupláltam és ellenőriztem; az alábbi 10 valódi találat marad,
súlyosság szerint.

> Megjegyzés: a `rbmap[MACRO][i]` „divergenciát" **cáfoltam** — a base is `operator[]`-t
> használ ([LteRealisticChannelModel.cc:652](../../src/simu5g/stack/phy/channelmodel/LteRealisticChannelModel.cc#L652)),
> tehát nem regresszió.

## Korrektségi

### 1. [közepes-magas] A háttér-/külső cella interferenciából eltűnik az antenna-nyereség, kábelveszteség és szögcsillapítás
[SionnaChannelModel.cc](../../src/simu5g/stack/phy/channelmodel/SionnaChannelModel.cc) — az
`initialize` globálisan nullázza `antennaGainEnB_`/`cableLoss_`-t, és a
`computeAngularAttenuation`→0 / `computeShadowing`→0 felülírás is globális. A
desired/in-scene linkeknél ez helyes (a Sionna path gain tartalmazza). DE a
`computeBackgroundCellInterference` **analitikus** terjedést használ (a bg cellák nincsenek
a scene-ben), és az is hívja a `computeAngularAttenuation`-t, plusz hozzáadja
`antennaGainEnB_`-t. Így a bg/ext interferencia ~18 dBi eNB-nyereséget + szöget + kábelt
**veszít** → alulbecsült interferencia minden olyan szcenárióban, ahol bg cella van (pl. a
MEC hálózatok `numBgCells`-szel). *Altitude:* a nullázás túl széles — a Sionna-specifikus
terheket csak az in-scene linkekre kéne alkalmazni.

### 2. [közepes] `sionna_rt.py`: egybeeső Tx/Rx (d=0) → nullával osztás → `inf` → érvénytelen JSON
[sionna_rt.py](../../src/simu5g/stack/phy/channelmodel/sionna/sionna_rt.py) `_two_ray_path_gain_db`
— az `1e-3` floor csak `ht`/`hr`-t védi, a `d_los`/`d_ref`-et nem. Két különálló, azonos
pozíciójú node → `d_los=0` → `exp(...)/d_los` = `inf` → `power_gain=inf` → a `json.dump`
(alapból `allow_nan=True`) **`Infinity`** tokent ír, amit a szigorú JSON (és a C++
`nlohmann`) elutasít → a tábla nem tölthető be. Javítás: nem-véges érték → `-300.0`
sentinel, és/vagy `allow_nan=False`.

### 3. [közepes] `sionna_rt.py`: a két backend eltérően helyezi a per-RB frekvenciákat (páros numBands)
A tworay `band_center_frequencies` szimmetrikus `(i-(n-1)/2)*bw`, a sionna backend a
`subcarrier_frequencies(n,scs)` = `(arange(n)-n//2)*scs` (páros n-re aszimmetrikus). Páros
sávszámnál (a séma példája is 6) a band-index→frekvencia leképezés **eltér** a két backend
között; mivel az `auto` némán vált, a commitolt tworay artifact és egy élő sionna-futás
per-sávban elcsúszik. (Gyakorlati hatás v1-ben kicsi, mert a path gain lassan változik
sávonként, de valódi inkonzisztencia.)

### 4. [közepes, latens] `SionnaManager`: `posToId_` pozíció-ütközés
[SionnaManager.cc](../../src/simu5g/stack/phy/channelmodel/sionna/SionnaManager.cc) —
`posToId_[posKey(pos)] = id` ütközés-ellenőrzés nélkül. Két **különböző**, mm-re kerekítve
azonos pozíciójú node (pl. egy helyre tett gNB+UE) egymásra íródik → az egyik koordinátából
rossz id-t old fel → rossz link / nullptr. (Az NR dupla-id UE véletlenül működik, mert
mindkét id-hez van link; valódi különálló egybeeső node-oknál törik.)

### 5. [közepes, latens] Hármas carrier-regisztrációs crash, ha a dekorátor/Sionna-modell eNB-n van
A `CompareChannelModel` (és a `SionnaChannelModel`) örökli a `LteChannelModel::initialize`-t,
ami a `cellInfo_->registerCarrier(...)`-t hívja; a compare két belső modellje (ref+cand)
**ugyanazt** a carriert regisztrálja ugyanazon a cellInfo-n → `CellInfo::registerCarrier`
„already exists" throw. UE-n cellInfo=null, ezért a szállított configok nem érintettek, de a
dekorátor mint általános eszköz nincs védve eNB-elhelyezés ellen.

### 6. [közepes] `SionnaManager::runGenerator`: idézőjelezetlen `std::system` + nincs kimenet-ellenőrzés
[SionnaManager.cc](../../src/simu5g/stack/phy/channelmodel/sionna/SionnaManager.cc) — a
parancs nyers string-konkatenáció: szóközös/metakarakteres út (pl. `pythonExecutable`,
`sionnaScript`, cacheDir) eltöri vagy **injektál**; a visszatérési érték wait-status (nem
nyers exit-kód); és rc==0 után rögtön `loadTableFromFile` fut **annak ellenőrzése nélkül**,
hogy a generátor írt-e kimenetet → félrevezető „cannot open" hiba.

### 7. [alacsony-közepes] `SionnaManager::loadTableFromFile`: védtelen JSON-elérés
A `.at("carriers")` / `.at("links")` / `.get<...>` hiányzó/null/rossz típusú mezőnél nyers
`nlohmann::json` kivételt dob (nem `cRuntimeError`-be csomagolva) → értelmetlen crash hibás
táblánál. (Szintén itt: a betöltött fájl `granularity`/`interferenceMode`-ja felülírja a
konfigot — szándékos, de a coupling-guard a régi értékkel futott.)

## Tisztaság / hordozhatóság / doc

### 8. [közepes] A README `sionnaize.py` példája törött hálózatot céloz
[README.md](../../simulations/nr/sionna/README.md) B-szekció: `sionnaize.py ... UrbanNetwork`
— de az `UrbanNetwork` **most már tartalmazza** a `hasSionnaManager`/`sionnaManager`
almodult, így a generált `UrbanNetworkSionna` wrapper **duplikálná** a submodult → NED
„already exists" hiba. A wrapper-út egyetlen dokumentált példája hibás (a wrapper csak a
kapcsoló **nélküli** hálózatokra — pl. `cars/Highway` — érvényes).

### 9. [közepes] Gép-specifikus abszolút utak a megosztott include-ban
[sionna-common.ini](../../simulations/nr/sionna/sionna-common.ini) és
[omnetpp.ini `SionnaLive`](../../simulations/nr/sionna/omnetpp.ini) —
`pythonExecutable`/`sionnaScript` `/home/zoli/...`-ra drótozva. A `sionna-common.ini` épp az
„include-old bárhonnan" megosztott fájl → bárki más checkoutján a tworay subprocess nem
indul. (A committed `[Config Sionna]` hordozható az artifacttal; az élő út nem.)

### 10. [alacsony] Elavult RNG-semlegesség komment
[CompareChannelModel.cc:93](../../src/simu5g/stack/phy/channelmodel/CompareChannelModel.cc#L93)
— „fading and shadowing disabled (… large-scale mode) these probes draw no RNG", de a
szállított `SionnaCompare` most **Full módban** (fading ON) fut; a semlegesség valójában a
per-node/per-TTI cache-eléssel áll fenn, nem a kikapcsolással. A komment hamis előfeltételt
állít → félrevezető egy jövőbeli módosításnál.

---

## Verifikáltan NEM hiba (a fals pozitívok elkerülésére)

- `getSINR` ág-/koordináta-/noiseFigure-választás mind a 4 esetben egyezik a base-szel; a
  desired pár pozíció-alapú + reciprok lookup → a végpont-sorrend nem számít.
- `setPhy`/`registerNode` időzítés: `sionnaManager_` POSTLOCAL-kor feloldva, a `setPhy`
  REGISTRATIONS2-kor fut → a guard teljesül.
- `getShadowingMap/getJakesMap` override init-előtti hívása: a `ModuleRefByPar::operator bool`
  nem dob feloldatlan refre.
- RNG-semlegesség **ténylegesen fennáll** (a probe + a belső getSINR azonos NOW-on, cache-elt
  húzás).
- A `<default("SionnaManager")> like ISionnaManager if hasSionnaManager` minta minden 17
  hálózatban feloldódik; `binder`/`carrierAggregation` hivatkozások helyesek.
- Nincs repo-szintű CLAUDE.md → nincs konvenció-sértés.

---

**Súlypont:** a #1 (bg/ext interferencia hibás Sionna alatt) és #2/#3 (sionna_rt.py edge-case
+ backend-konzisztencia) a leginkább érdemi korrektségi pontok; a #8/#9 a felhasználót
közvetlenül érintő doc/hordozhatóság. Egyik sem blokkolja a jelenlegi (single-cell, statikus,
committed-artifact) használatot — a fingerprint-suite 129 OK ezt mutatja —, de
bg-cellás/multi-carrier/élő-sionna kiterjesztésnél felszínre jönnek.
