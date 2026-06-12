#include <Fuzzy.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <RBDdimmer.h>

// Konfigurasi pin
// 1. Sensor suhu ds18b20 
#define ONE_WIRE_BUS 4

// 2. Modul dimmer
#define outputPin    3 // D1
#define zerocross    2 // Z-C

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

dimmerLamp acDimmer(outputPin); 

Fuzzy *fuzzy = new Fuzzy();

float setpoint = 37.0;
float lastError = 0;

void setup() {
  Serial.begin(115200);
  sensors.begin();
  
  acDimmer.begin(NORMAL_MODE, ON); 

  // Variabel input dan output
  FuzzyInput* error = new FuzzyInput(1);
  FuzzyInput* deltaError = new FuzzyInput(2);
  FuzzyOutput* dayaHeater = new FuzzyOutput(1);

  // 2 MF input Error
  FuzzySet* errorNegatif = new FuzzySet(-5, -5, -2, 2);
  FuzzySet* errorPositif = new FuzzySet(-2, 2, 8, 8);
  
  // 2 MF input Delta Error
  FuzzySet* DE_Negatif = new FuzzySet(-5, -5, -1, 1);
  FuzzySet* DE_Positif = new FuzzySet(-1, 1, 3, 3);
  
  // 2 MF Output Daya Heater (0-100)
  FuzzySet* kecil  = new FuzzySet(0, 0, 10, 20); 
  FuzzySet* sedang = new FuzzySet(15, 25, 25, 35); 
  FuzzySet* besar  = new FuzzySet(30, 70, 100, 100);
  
  error->addFuzzySet(errorNegatif);
  error->addFuzzySet(errorPositif);
  deltaError->addFuzzySet(DE_Negatif);
  deltaError->addFuzzySet(DE_Positif);

  dayaHeater->addFuzzySet(kecil);
  dayaHeater->addFuzzySet(sedang);
  dayaHeater->addFuzzySet(besar);

  fuzzy->addFuzzyInput(error);
  fuzzy->addFuzzyInput(deltaError);
  fuzzy->addFuzzyOutput(dayaHeater);

  // Rule Base 
  // Rule 1: Jika Error (+/dingin) & dError (+/turun) = Daya Besar
  FuzzyRuleAntecedent* ifErrPosAndDePos = new FuzzyRuleAntecedent();
  ifErrPosAndDePos->joinWithAND(errorPositif, DE_Positif);
  FuzzyRuleConsequent* thenDayaBesar = new FuzzyRuleConsequent();
  thenDayaBesar->addOutput(besar);

  // Rule 2: Jika Error (+/dingin) & dError (-/naik) = Daya Sedang
  FuzzyRuleAntecedent* ifErrPosAndDeNeg = new FuzzyRuleAntecedent();
  ifErrPosAndDeNeg->joinWithAND(errorPositif, DE_Negatif);
  FuzzyRuleConsequent* thenDayaSedang = new FuzzyRuleConsequent();
  thenDayaSedang->addOutput(sedang);

  // Rule 3: Jika Error (-/panas) & dError (+/turun) = Daya Kecil
  FuzzyRuleAntecedent* ifErrNegAndDePos = new FuzzyRuleAntecedent();
  ifErrNegAndDePos->joinWithAND(errorNegatif, DE_Positif);
  FuzzyRuleConsequent* thenDayaKecil1 = new FuzzyRuleConsequent();
  thenDayaKecil1->addOutput(kecil);
  
  // Rule 4: Jika Error (-/panas) & dError (-/naik) = Daya Kecil
  FuzzyRuleAntecedent* ifErrNegAndDeNeg = new FuzzyRuleAntecedent();
  ifErrNegAndDeNeg->joinWithAND(errorNegatif, DE_Negatif);
  FuzzyRuleConsequent* thenDayaKecil2 = new FuzzyRuleConsequent();
  thenDayaKecil2->addOutput(kecil);

  fuzzy->addFuzzyRule(new FuzzyRule(1, ifErrPosAndDePos, thenDayaBesar));
  fuzzy->addFuzzyRule(new FuzzyRule(2, ifErrPosAndDeNeg, thenDayaSedang));
  fuzzy->addFuzzyRule(new FuzzyRule(3, ifErrNegAndDePos, thenDayaKecil1));
  fuzzy->addFuzzyRule(new FuzzyRule(4, ifErrNegAndDeNeg, thenDayaKecil2));
  
  delay(5000); 
}

void loop() {
  sensors.requestTemperatures(); 
  float temp = sensors.getTempCByIndex(0);

  // Mengatasi pembacaan suhu yang tidak tepat
  if (temp <= -100 || temp >= 80) return;

  // Hitung error dan delta error
  float errorVal = setpoint - temp;
  float dErrorVal = errorVal - lastError;

  // Proses Fuzzyfication
  fuzzy->setInput(1, errorVal);
  fuzzy->setInput(2, dErrorVal);
  fuzzy->fuzzify();

  // Defuzzyfication (Centroid)
  float output = fuzzy->defuzzify(1);

  // Eksekusi ke Dimmer (0-100)
  acDimmer.setPower((int)output);

  unsigned long detik = millis() / 1000;
  Serial.print("Waktu(s): "); Serial.print(detik);
  Serial.print(" | Suhu: "); Serial.print(temp);
  Serial.print(" | Err: "); Serial.print(errorVal);
  Serial.print(" | dErr: "); Serial.print(dErrorVal);
  Serial.print(" | Out Dimmer: "); Serial.println(output);

  lastError = errorVal;
  
  // Delay 10 detik setiap pengambilan data
  delay(10000); 
}