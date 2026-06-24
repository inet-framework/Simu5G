# Report: a Simu5G kereszt-modell shadowing/fading csatolása és a `CompareChannelModel`

Státusz: leírás / tanulság
Kapcsolódó commit: `6a17f786` — *Sionna: make CompareChannelModel transparent (re-parent to realistic model)*
Kapcsolódó terv: [Plan A §7](sionna-channel-plan.md) (validációs dekorátor)

## Összefoglaló (TL;DR)

A Simu5G beépített csatornamodellje (`LteRealisticChannelModel` és leszármazottai)
a DL CQI számításakor **átnyúl a peer (UE) csatornamodelljébe** a shadowing- és
Jakes-fading-állapotért, és ehhez a polimorf `LteChannelModel*` pointert
`dynamic_cast<LteRealisticChannelModel*>`-tel a konkrét analitikus típusra castolja.
Ez egy **szivárgó absztrakció / szoros csatolás**: az analitikus belső állapot nem
része a csatorna-interfésznek, mégis egy másik példányból kell elérni.

Emiatt egy *dekorátor* csatornamodell (`CompareChannelModel`), ami az
`LteChannelModel`-ből származott, a cast miatt **null-t** kapott → **segfault /
throw**. A megoldás: a dekorátort az `LteRealisticChannelModel`-ből származtatni, és
a térkép-accessorokat a belső reference-modellre forwardolni.

---

## 1. A mechanizmus, ami a castot indokolja

A shadowing és a Jakes-fading **állapotos és UE-specifikus**:

- **Shadowing:** időben korrelált (Exponential Average Window), UE-nként egy
  `ShadowFadingMap`-ben tárolva, ami a futás során fejlődik.
- **Jakes-fading:** a beérkezési szögek UE-nként **egyszer** inicializálódnak
  véletlennel (`uniform`, `exponential`), utána determinisztikus időfüggvény.

Ez az állapot a **UE-oldali** csatornamodell-példányban él
(`lastComputedSF_`, `jakesFadingMap_`).

A konfliktus forrása: a **DL CQI-t az eNB számolja** (a saját csatornamodelljén fut
a `getSINR`), de a DL-irányú shadowing/fading az, **amit a UE tapasztal**. Ahhoz,
hogy az eNB által becsült CQI **konzisztens** legyen azzal, amit a UE ténylegesen lát
a vételkor, az eNB-nek a **UE map-jét** kell használnia. A kód kommentje ezt ki is
mondja ([LteRealisticChannelModel.cc:196](../../src/simu5g/stack/phy/channelmodel/LteRealisticChannelModel.cc#L196)):

```cpp
double LteRealisticChannelModel::computeShadowing(..., bool cqiDl)
{
    ShadowFadingMap *actualShadowingMap;
    if (cqiDl) // if we are computing a DL CQI we need the Shadowing Map stored on the UE side
        actualShadowingMap = obtainShadowingMap(nodeId);   // <- átnyúlás a peer-be
    else
        actualShadowingMap = &lastComputedSF_;             // saját (UL / saját oldal)
    ...
}
```

## 2. Hol és hogyan downcastol

A peer-állapot elérése a polimorf `getChannelModel()`-en keresztül történik, ami a
bázis `LteChannelModel*`-ot adja vissza — de a `getShadowingMap()`/`getJakesMap()`
csak az `LteRealisticChannelModel`-en létezik (az analitikus modell belső állapota),
ezért **le kell castolni**:

[LteRealisticChannelModel.cc:2577-2595](../../src/simu5g/stack/phy/channelmodel/LteRealisticChannelModel.cc#L2577) — `obtainShadowingMap`:
```cpp
LtePhyBase *phy = <UE phy a binderből, ueId alapján>;
if (phy == nullptr) return nullptr;
LteRealisticChannelModel *re = dynamic_cast<LteRealisticChannelModel*>(phy->getChannelModel(carrierFrequency_));
ShadowFadingMap *j = re->getShadowingMap();   // <- NINCS null-check a re-re!
return j;
```

[LteRealisticChannelModel.cc:2551-2575](../../src/simu5g/stack/phy/channelmodel/LteRealisticChannelModel.cc#L2551) — `obtainUeJakesMap` (itt van explicit ellenőrzés):
```cpp
LteRealisticChannelModel *re = dynamic_cast<LteRealisticChannelModel*>(phy->getChannelModel(carrierFrequency_));
if (re == nullptr)
    throw cRuntimeError("... channel model is a null pointer");
return re->getJakesMap();
```

Ugyanez a downcast-minta megvan az **interferencia-számításban** is
([LteRealisticChannelModel.cc:2629](../../src/simu5g/stack/phy/channelmodel/LteRealisticChannelModel.cc#L2629)):
a szomszéd cella `phy->getChannelModel(...)`-ját `LteRealisticChannelModel*`-ra
castolja, hogy a `getAttenuation()`-jét hívja. Tehát a csatolás **nem egyedi**: a
beépített modell több ponton is feltételezi, hogy a *peer* csatornamodellje
`LteRealisticChannelModel`.

## 3. Hogyan manifesztálódott (a tünet)

A Plan A §7 `CompareChannelModel` dekorátor a UE csatornamodellje. Amikor az eNB
beépített modellje DL CQI-t számolt és a UE shadowing/jakes map-jéért nyúlt:

```cpp
re = dynamic_cast<LteRealisticChannelModel*>(<a UE CompareChannelModel-je>);
// CompareChannelModel : public LteChannelModel  →  re == nullptr
re->getShadowingMap();   // obtainShadowingMap: null-deref → SEGFAULT
                         // obtainUeJakesMap : explicit throw
```

A crash a futás init-fázisában jelentkezett (cell-attach DL CQI), a
`...ue[*].cellularNic.nrPhy`-ben. Korábban ezt a `CompareChannelModel`-szcenárióban
a beépített shadowing/fading **kikapcsolásával** kerültük meg (a §7 „large-scale only"
módja) — de ettől a fizika eltért a beépített baseline-tól, így a fingerprint nem
egyezett (FAILED, nem PASS).

## 4. A választott javítás

Hogy a dekorátor *transzparens* legyen (fading ON, beépített baseline-egyezés), a
meglévő csatolási „szerződéshez" idomítottuk:

1. **A két accessor virtuálissá** az `LteRealisticChannelModel`-ben
   ([LteRealisticChannelModel.h:396,401](../../src/simu5g/stack/phy/channelmodel/LteRealisticChannelModel.h#L396)):
   ```cpp
   virtual JakesFadingMap   *getJakesMap()     { return &jakesFadingMap_; }
   virtual ShadowFadingMap  *getShadowingMap() { return &lastComputedSF_; }
   ```
2. **A dekorátor az `LteRealisticChannelModel`-ből származik**
   ([CompareChannelModel.h:26](../../src/simu5g/stack/phy/channelmodel/CompareChannelModel.h#L26)) →
   a `dynamic_cast` **sikerül** (nem null).
3. **A két accessort a belső reference-modellre forwardolja**
   ([CompareChannelModel.h:70,80](../../src/simu5g/stack/phy/channelmodel/CompareChannelModel.h#L70)):
   ```cpp
   ShadowFadingMap *getShadowingMap() override {
       if (reference_) { auto *r = dynamic_cast<LteRealisticChannelModel*>(reference_.get());
                         if (r) return r->getShadowingMap(); }
       return LteRealisticChannelModel::getShadowingMap();
   }
   ```
   Így az eNB és a UE-oldali reference **ugyanazt a korrelált térképet** osztja, mint
   egy sima beépített futásban.

**Eredmény (bizonyítva):** `SionnaCompare` vs. egy tiszta beépített `BuiltinRef`
iker — a **tplx és ~tNl bitre egyezik** (`55bf-1704` / `b62a-c0f4`), csak az **sz**
tér el (a logolt Sionna-delták). Teljes fingerprint-suite: 129 OK. A base-accessorok
virtuálissá tétele a többi tesztre **inert** (nincs viselkedés-változás).

## 5. Kompromisszumok és tisztább alternatívák

A javítás **idomulás** a meglévő csatoláshoz, nem annak megszüntetése. Hátrányai:

- A dekorátor egy teljes `LteRealisticChannelModel`-t **cipel** (shadowing/jakes
  tagok, interferencia-rutinok, `getSINR`-impl), amiből szinte semmit nem használ
  (mindent a két belső modellnek továbbít).
- **A base osztályt piszkálni kellett** (2 accessor virtuálissá).

Tisztább (de invazívabb) megoldások, ha valaha refaktorálnák a csatolást:

1. **Az interfész tegye közzé a térképeket:** `getShadowingMap()`/`getJakesMap()`
   (vagy egy szűkebb „large-scale state" interfész) kerüljön az `ILteChannelModel` /
   `LteChannelModel` szintjére → nincs downcast, a dekorátor tisztán forwardol.
2. **Explicit állapotátadás:** a DL CQI számítás kapja meg paraméterként a UE
   large-scale állapotát, ahelyett hogy a peer példányba nyúlna — megszüntetné a
   példányok közti rejtett függést.
3. **Az állapot kiszervezése** egy külön, csatornamodell-független tárolóba (pl. a
   `Binder`-ben vagy egy „large-scale state" szolgáltatásban), amit mindkét végpont
   ugyanúgy elér — így a modell maga állapotmentes lookup lehetne.

## 6. Tanulság

A `dynamic_cast` nem önkényes: egy valós **konzisztencia-igényt** (a DL CQI ugyanazt
a korrelált shadowing/fading realizációt használja, amit a UE lát) old meg egy
**szivárgó absztrakción** keresztül. Bármilyen *dekorátor vagy alternatív*
csatornamodell, ami egy link egyik végén áll, miközben a másik végén beépített modell
van, **bele fog ütközni** ebbe — vagy idomul hozzá (mint most), vagy a beépített
nagyléptékű effektusok kikapcsolásával kerüli meg. Új csatornamodell tervezésekor ezt
a csatolást előre számításba kell venni (a kódban legalább 3 downcast-hely:
`obtainShadowingMap`, `obtainUeJakesMap`, `computeDownlinkInterference`).
