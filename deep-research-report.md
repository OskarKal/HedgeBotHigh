# Specyfikacja techniczna – Bot hedgingowy opcyjno-futures

## Streszczenie wykonawcze  
Projekt zakłada stworzenie automatycznego bota hedgingowego w C++ wykorzystującego zaawansowane modele wyceny i algorytmy hedgingu opcyjnego oraz kontrakty futures. Architektura systemu uwzględnia moduły: pobierania danych (rynku i historycznych), normalizacji, wyceny opcji (BSM, lokalna zmienność, Heston, skokowo-dyfuzyjny Mertona), kalibracji modeli, kalkulator greków, silnik hedgingu, symulator brokera/egzekucji, zarządzanie ryzykiem, backtester, symulator Monte Carlo, system logowania/monitoringu oraz pipeline CI/CD. Zalecane modele to **Black–Scholes–Merton**, **Dupire (model lokalnej zmienności)**, **Heston 1993** oraz **Merton jump-diffusion 1976**【27†L153-L161】【41†L3101-L3109】. Należy uwzględnić równania tych modeli, parametry i metody kalibracji (optymalizacja wielowymiarowa, MLE itp.). Przedział czasu kalibracji wyniesie np. 30 dni z aktualizacją co miesiąc lub przy znaczących zmianach rynkowych. Do obliczeń numerycznych użyjemy technik takich jak FFT, COS, różnic skończonych, Monte Carlo z redukcją wariancji – zalecane biblioteki to Eigen, Boost, Intel MKL i QuantLib (C++).  

W części hedgingowej zaimplementujemy strategię **delta/gamma/vega-hedging**, rebalansowaną cyklicznie (co godzinę/godzinę) lub przy przekroczeniu progów (np. zmiana delty o 5%), z uwzględnieniem pochylenia beta-futures i spreadów/collarów ochronnych【68†L270-L278】【69†L75-L84】. Transakcje będą uwzględniały model kosztów i slippage (np. stała stawka + procent, zaimplementowana w symulatorze brokera). Kontrola ryzyka obejmuje limity pozycji, obliczenia VaR/ES, obsługę depozytu zabezpieczającego i stop-loss, a także system „circuit breaker”.  

W fazie testów przeprowadzimy backtesty historyczne i symulacje Monte Carlo: wykorzystane dane obejmą kursy spot, dane tickowe, surface zmienności opcji oraz stopy procentowe, z próbnikami od minuty do dnia. Modele będą kalibrowane "walk-forward" (np. co 30 dni), a efekty oceniamy wskaźnikami Sharpe’a, max drawdown, hit rate, rozkładem P&L i miarami tail-risk (ES)【69†L59-L65】. Przewidujemy zestaw prac do realizacji: analiza wymagań, implementacja modułów (dane, wycena, greki, hedging, ryzyko, testy, CI), walidacja (testy jednostkowe/integracyjne) i wdrożenie kontenerowe. Planowane rezultaty to pełna baza kodu C++ (z dokumentacją API), pliki konfiguracyjne (YAML), skrypty backtestu, Dockerfile i pipeline CI/CD. Wszystkie niewymienione parametry (kapitał, rynki) potraktujemy jako założenia konfiguracyjne.

## Lista implementacyjna (priorytet + nakład)  
1. **Zbieranie i przygotowanie danych historycznych** (niski) – pozyskać kursy spot, ceny opcji (surface) oraz stopy procentowe; przygotować skrypty ETL.  
2. **Projekt architektury i API bota** (średni) – zaplanować moduły, interfejsy klas i strukturę kodu.  
3. **Moduł pobierania i normalizacji danych** (średni) – klasy `MarketDataFetcher`, `DataNormalizer` (np. interpolacja vol surface).  
4. **Wdrożenie bibliotek numerycznych** (niski) – skonfigurować Eigen, Boost, Intel MKL, QuantLib; utworzyć szkielet CMake/Make.  
5. **Moduł wycen opcji (Pricery)** (wysoki) – klasy `BSM_Pricer`, `LocalVol_Pricer`, `Heston_Pricer`, `MertonJump_Pricer` implementujące wzory z literatury【27†L153-L161】【41†L3101-L3109】.  
6. **Moduł kalkulacji greków** (średni) – klasy/metody do ∂V/∂S, ∂²V/∂S², ∂V/∂σ (np. różnicowanie numeryczne lub analityczne).  
7. **Moduł kalibracji modeli** (wysoki) – klasa `ModelCalibrator`, realizująca optymalizacje (Levenberg–Marquardt, Nelder–Mead, PSO itp.) dla każdego modelu.  
8. **Silnik hedgingu** (wysoki) – klasa `HedgingEngine` realizująca wyliczanie pozycji delta/gamma/vega-hedge, strategię spreadów/collarów, beta-weighting futures【69†L75-L84】【68†L270-L278】, decyzje rebalansowania.  
9. **Moduł symulowanego brokera/egzekucji** (średni) – `BrokerSimulator` uwzględniający prowizje, slippage i śledzący P&L.  
10. **Moduł zarządzania ryzykiem** (wysoki) – `RiskManager` obliczający VaR/ES, kontrolujący wielkość pozycji, margin call, stop-loss i circuit breaker.  
11. **Backtester i symulacje Monte Carlo** (wysoki) – klasy `Backtester`, `MonteCarloSimulator` do testów strategii na danych historycznych i losowych ścieżkach.  
12. **Testy jednostkowe i integracyjne** (średni) – opracowanie testów pokrywających każdy moduł (np. zgodność formuł, stabilność kalibracji).  
13. **CI/CD i wdrożenie kontenerowe** (średni) – konfiguracja pipeline (np. GitLab CI), stworzenie Dockerfile obrazów (budowa, testy, deploy).  
14. **Dokumentacja i przykłady** (średni) – przygotowanie specyfikacji kodu, plików konfiguracyjnych (YAML) i przykładowych wyników backtestów.  

## Specyfikacja techniczna  

**Moduły i klasy:**  
- `MarketDataFetcher` – ładowanie danych z plików czy API (kursy, opcje, stopy). Funkcje: `loadPriceData()`, `loadOptionSurface()`.  
- `DataNormalizer` – oczyszczanie, interpolacja ubytków i normalizacja time series.  
- **Wycenianie opcji:** Klasy dziedziczące po abstrakcji `OptionPricer`; np.:  
  ```cpp
  class OptionPricer {
  public:
      virtual double price(const OptionData& opt, const MarketData& md) = 0;
      virtual double delta(const OptionData&, const MarketData&) { /* opcjonalnie */ }
      // ...
  };
  class BSM_Pricer : public OptionPricer { /* implementuje formułę Black-Scholesa */ };
  class LocalVol_Pricer : public OptionPricer { /* korzysta z formuły Dupire’a */ };
  class Heston_Pricer : public OptionPricer { /* CF integral (Heston 1993) */ };
  class MertonJump_Pricer : public OptionPricer { /* model jump-diffusion */ };
  ```  
  Każdy pricer odwołuje się do parametrów modelu (σ, κ, θ, ξ, ρ, λ, μ_j, σ_j itp.) i używa odpowiednich metod numerycznych (FFT, COS, FDM, Monte Carlo). Wzory modeli:  
  - **BSM:** \(dS_t = \mu S_t\,dt + \sigma S_t\,dW_t\). Wartość opcji European call: formuła zamknięta【27†L153-L161】.  
  - **Model lokalnej zmienności (Dupire):** \(\partial C/\partial T = \tfrac12\sigma^2(K,T)K^2\partial^2C/\partial K^2 - (r-d)K\partial C/\partial K - dC\)【53†L194-L199】.  
  - **Model Hestona:** \(dS_t = rS_tdt + \sqrt{v_t}S_tdW^S_t,\; dv_t = \kappa(\theta-v_t)dt + \xi\sqrt{v_t}dW^v_t\), \(dW^S\,dW^v = \rho dt\)【41†L3101-L3109】.  
  - **Model Mertona (skoki):** \(dS_t = rS_tdt + \sigma S_tdW_t + S_{t-}dJ_t\), gdzie \(J_t\) to proces Poissona ze średnią \(\lambda\) i rozkładem wielkości skoku (lognormalna). Formuły zgodnie z Mertonem (1976)【56†L369-L377】.  

- `ModelCalibrator` – realizuje kalibrację parametrów do danych rynkowych. Dla BSM dopasowuje σ do cen opcji (Least Squares). Dla Hestona/Dupire/Jump stosuje zoptymalizowane funkcje celu (min. różnicę między cenami modelu a rynkowymi). Metody optymalizacji: Levenberg–Marquardt, Nelder–Mead, globalne (PSO, genetyczny), MLE. Przydatne algorytmy z bibliotek Eigen/Boost. Należy uwzględnić ograniczenia zakresu parametrów (np. \(\sigma\in(0,1]\), \(\kappa>0,\;\theta>0,\;\rho\in[-1,1]\) itp.) oraz sensowne zgadnane wartości początkowe. Kalibrację wykonuje się okresowo (np. co 30 dni lub po dużych ruchach vol).  

- `GreeksCalculator` – obliczanie wrażliwości portfela: ∂V/∂S (delta), ∂²V/∂S² (gamma), ∂V/∂σ (vega) itp. Wykorzystuje metody analityczne (jeśli dostępne) lub różnicowanie numeryczne.  

- `HedgingEngine` – wdraża reguły hedgingu:  
  - **Delta/gamma/vega hedging:** oblicza wymagane pozycje w bazowych aktywach/opcjach, aby zneutralizować delta/gamma/vega.  
  - **Rebalansowanie:** decyzja o rehedgu przy przekroczeniu progów (np. |Δ_new - Δ_old| > próg) lub cyklicznie (np. co 1h).  
  - **Futures beta-weighting:** ustalanie liczby kontraktów futures do hedgingu ekspozycji delta (patrz przykład w [69†L75-L84]).  
  - **Spready/collars:** tworzenie strategii kombinacyjnych (kupno put + sprzedaż call itp.) w celu limitowania zysków/strat zgodnie z założeniami (np. *protective collar*【68†L270-L278】).  

- `BrokerSimulator` – symulator wykonania zleceń (papierowego handlu), uwzględniający opłaty i poślizgi. Udostępnia interfejsy: `sendOrder()`, `getPosition()`, `getPnL()`.  

- `RiskManager` – monitoruje ekspozycję: oblicza aktualny **VaR/ES** portfela (np. metodą historyczną lub parametryczną), pilnuje limitów margin, nakłada stop-loss’y i „circuit breaker”.  

- `Backtester` – odgrywa strategię na danych historycznych w ustalonym okresie, rekalibrując parametry według harmonogramu *walk-forward*. Gromadzi metryki: Sharpe, maks. obsunięcie, hit rate, tail-risk.  

- `MonteCarloSimulator` – generuje losowe ścieżki cen (geometr. Browna, Hestona itp.) i symuluje działanie strategii w warunkach modelowych oraz scenariuszach stresowych (skoki, zmienne kor.) dla analizy wrażliwości.  

- `Logger/Monitor` – zbiera logi operacyjne, metryki działania (czasy wykonania, użycie pamięci), udostępnia punkty monitoringu (np. REST API lub pliki).  

**Metody numeryczne:** Wykorzystujemy techniki przyspieszające: FFT lub metoda COS (powodują szybsze liczenie transformacji Fouriera przy wycenie opartej na funkcji charakterystycznej modelu), dyskretne metody PDE (np. finite differences) do wyceny opcji egzotycznych, symulacje Monte Carlo (z antithetic variates, quasi-MC) do weryfikacji. Biblioteka QuantLib może służyć za gotowe narzędzia do kalibracji i wyceny. Całość pisana w C++ zgodnie z wymaganiem.  

**Tabela 1. Porównanie modeli wyceny – kryteria:**  

| Model          | Typ zmienności    | Zalety                                         | Wady/założenia                     | Parametry (kalibracja)                |
|----------------|-------------------|------------------------------------------------|------------------------------------|---------------------------------------|
| **Black–Scholes**【27†L153-L161】  | Stała σ         | Prosty, zamknięte formuły, szybki (η niski)   | Nieuwzględnia smile, stała ρ=0     | σ (jedyna zmienna), kalibracja z IV  |
| **Local Vol (Dupire)**【53†L194-L199】 | σ(S,t)        | Dokładnie fit surface, pasuje do danych       | Skomplikowana kalibracja (dane surface) | Cała powierzchnia σ(S,t) z formuły Dupire’a |
| **Heston (1993)**【41†L3101-L3109】 | Stoch. (v_t)   | Modeluje przekrzywienie, smile, związek S-σ  | Kalibracja 5 parametrów (κ,θ,ξ,ρ,v0) | Parametry → dopasowanie funkcji cen  |
| **SABR**       | Stoch. (α,β,ρ,ν)  | Elastyczny (bond/fx), dobra formula atm       | Brak zamkniętej formuły vol smile   | α,β,ρ,ν: kalibracja do ATM i vol curve |
| **Merton Jump-Diffusion**【56†L369-L377】 | Skoki (λ,μ,σ) | Skoki → lepokurtyczność, uśmiech vol          | Bardzo złożona analiza, wielowymiarowa kalib. | λ (intensity), μ_j, σ_j (size) – dopas. do smile  |

**Tabela 2. Porównanie strategii hedgingowych – ryzyko i koszt:**  

| Strategia hedgingu          | Zasada działania                              | Zalety                             | Wady                               | Koszt wdrożenia  |
|-----------------------------|-----------------------------------------------|------------------------------------|------------------------------------|------------------|
| Delta-neutral + futures【69†L75-L84】 | Delta-hedge portfela + krótkie futures  | Niska wrażliwość na krótkie ruchy, prostota | Potrzeba stałej rebalancji       | Średni          |
| Gamma scalping + dyn. rebal | Częste rebal. dla zbierania premii gamma      | Może zarabiać na zmienności        | Bardzo wysoki turnover, koszty     | Wysoki          |
| Protective collar【68†L270-L278】      | Long aktywo + kupno put + sprzedaż call | Ograniczony downside, niskie koszty | Ogranicza potencjalne zyski     | Niski            |
| Spread kalendarzowy         | Kupno/sprzedaż opcji z różnymi expiry         | Wyrównuje term structure vol       | Złożona administracja spreadów    | Średni          |

```mermaid
flowchart TB
    subgraph "Moduły systemu"
      A[MarketDataFetcher] --> B[DataNormalizer]
      B --> C[Pricers (BS, Heston, ...)]
      C --> D[ModelCalibrator]
      D --> E[GreeksCalculator]
      E --> F[HedgingEngine]
      F --> G[BrokerSimulator]
      F --> H[RiskManager]
      G --> I[Backtester]
      G --> J[MonteCarloSimulator]
      H --> I
      I --> K[Logger/Monitor]
    end
```
*Diagram: Architektura systemu – moduły bota i relacje między nimi.*

## Przepisy kalibracji modeli  

**Black–Scholes (BSM):**  
1. Zbierz ceny opcji (rynek) dla różnych strike’ów i terminów.  
2. Cel: minimalizuj sumę kwadratów różnicy między cenami rynkowymi a modelem. Funkcja celu: \(\sum_i (C_{\rm model}(\sigma) - C_{\rm market})^2\).  
3. Użyj optymalizacji (np. Levenberg–Marquardt) z parametrem \(\sigma\in(0,1]\). Ustaw początkową \(\sigma\approx 0.2\).  
4. (Opcjonalnie) Kalibruj oddzielnie dla różnych dat wygaśnięcia i wygładź. 

**Model lokalnej zmienności (Dupire):**  
1. Zbuduj powierzchnię IV z rynku.  
2. Oblicz pochodne cen opcji (przez gładzenie IV lub finite differences).  
3. Zastosuj formułę Dupire’a: \(\sigma^2(K,T) = \frac{2\partial_T C + 2(r-d)K\partial_KC}{K^2 \partial^2_{K}C}\) (pochodne liczone numerycznie【53†L194-L199】).  
4. Wynikiem jest funkcja \(\sigma(S,t)\). Użyj regularizacji (np. penalty na ostre zmiany) i algorytmu globalnego (np. grid search + lokalna opt.).  

**Model Hestona:**  
1. Zbierz ceny opcji (cała surface) i dane rynkowe \(S_0,r\).  
2. Funkcja celu: minimalizuj błąd implied vol lub cenę opcji. Preferuj MLE (maksymalna wiarygodność) lub least-squares.  
3. Metoda: Kombinacja globalnego podejścia (PSO, genetyczny) + lokalnej optymalizacji (Nelder–Mead). Parametry: \(\kappa>0,\;\theta>0,\;\xi>0,\;v_0>0,\;\rho\in[-1,1]\). Zakresy np. \(\kappa\in(0,10)\), \(\theta\in(0,1)\), \(\xi\in(0,5)\). Startuj od ostrożnych guessów (np. \(\kappa=1,\theta=0.2,\xi=0.5,\rho=-0.5\)).  
4. Powtarzaj co miesiąc lub gdy zmienność się zmieni. Sprawdzaj zbieżność (np. gradient < ε).  

**Model Mertona (jump):**  
1. Dane: vol surface. Parametry: intensywność \(\lambda\), średnia skoku \(\mu_J\), st. odch. skoku \(\sigma_J\).  
2. Cel: dopasuj funkcję gęstości ceny tak, by odtworzyć obserwowany uśmiech vol【56†L369-L377】. Funkcja kosztu: sum-of-squares cen opcji.  
3. Optymalizacja globalna (np. differential evolution) z nawiasami: \(\lambda\in(0,1)\), \(\mu_J\in(-0.5,0.5)\), \(\sigma_J\in(0,2)\). Początkowo mała intensywność \(\lambda\approx 0.1\).  
4. Ewentualnie wykorzystaj metodę MLE na stopniach swobody rozkładu cen skoków (jeśli dostępne) jako inicjalizację.  

## Przykłady testów i scenariuszy backtestów  

**Testy jednostkowe:**  
- **Pricer BSM:** sprawdź cenę call/put z BSM na znanych przypadkach (np. put-call parity dla opcji europejskich)【27†L153-L161】.  
- **Pricer Heston:** porównaj z biblioteką referencyjną (np. QuantLib) przy standardowych parametrach (możliwość napisania testu z podstawowym Heston CF).  
- **Greek Calculator:** upewnij się, że delta-gamma pokrywają się z różniczkowaniem numerycznym.  
- **Kalibrator:** sprawdź, czy metoda przywraca prawidłowe parametry na danych syntetycznych (np. wprowadź znane parametry, wygeneruj ceny i odkalibruj).  
- **Hedging Engine:** zweryfikuj, że przy prostym ruchu ceny pozycje hedgingowe zmieniają się zgodnie z oczekiwaniami (np. po wzroście S delta powinna rosnąć).  

**Backtest / symulacje:**  
- Strategia testowana na indeksie S&P500 w 2025 (ew. WIG20), przy danych minutowych. Mierzona wydajność: Sharpe, max DD.  
- Scenariusz: nagły 10% spadek rynku – ocenić ograniczenie strat (np. poprzez protective collar).  
- Symulacja MC: 10k ścieżek (t=252 dni, dt=1 dzień) pod model Heston i porównanie rozkładów P&L.  

## Przykładowe pliki konfiguracyjne i CI/CD  

**Konfiguracja (YAML):** Ustawienia strategii, ścieżki do danych i parametrów modeli. Na przykład:  
```yaml
bot_config:
  data_path: "/data/market"
  models:
    BSM: {rate: 0.01}
    Heston: {kappa:1.0, theta:0.2, xi:0.5, rho:-0.5}
  calibration:
    window_days: 30
    method: "LevenbergMarquardt"
  hedging:
    rebalance_interval: 3600   # seconds
    delta_threshold: 0.05
  execution:
    commission: 0.001         # 0.1% fee
    slippage_model: "spread+0.001"
```

**Pipeline CI/CD (np. GitLab CI):** Automatyczne budowanie i testy. Przykład:  
```yaml
stages:
  - build
  - test
  - deploy

build:
  stage: build
  script:
    - mkdir build && cd build
    - cmake .. -DCMAKE_BUILD_TYPE=Release
    - make -j$(nproc)

test:
  stage: test
  script:
    - cd build
    - make test

deploy:
  stage: deploy
  script:
    - docker build -t hedging-bot:latest .
    - docker push registry/hedging-bot:latest
```

