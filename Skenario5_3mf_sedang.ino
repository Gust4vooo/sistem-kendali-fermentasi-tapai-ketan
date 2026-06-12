#include <Fuzzy.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <RBDdimmer.h>

// Konfigurasi pin
#define ONE_WIRE_BUS 4

// 1. Sensor suhu ds18b20
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
  
  // 3 MF input Error
  FuzzySet* errorNegatif = new FuzzySet(-5, -5, -2, 0); 
  FuzzySet* errorNol     = new FuzzySet(-2, 0, 0, 2);   
  FuzzySet* errorPositif = new FuzzySet(0, 2, 8, 8); 
  
  error->addFuzzySet(errorNegatif);
  error->addFuzzySet(errorNol);
  error->addFuzzySet(errorPositif);

  // 3 MF input Delta Error
  FuzzySet* DE_Negatif = new FuzzySet(-5, -5, -1, 0);
  FuzzySet* DE_Nol     = new FuzzySet(-1, 0, 0, 1);
  FuzzySet* DE_Positif = new FuzzySet(0, 1, 3, 3);
  
  deltaError->addFuzzySet(DE_Negatif);
  deltaError->addFuzzySet(DE_Nol);
  deltaError->addFuzzySet(DE_Positif);

  // MF Output Daya Heater (0-100)
  FuzzySet* kecil  = new FuzzySet(0, 0, 10, 20);
  FuzzySet* sedang = new FuzzySet(15, 25, 25, 35); 
  FuzzySet* besar  = new FuzzySet(30, 70, 100, 100);
  
  dayaHeater->addFuzzySet(kecil);
  dayaHeater->addFuzzySet(sedang);
  dayaHeater->addFuzzySet(besar);

  fuzzy->addFuzzyInput(error);
  fuzzy->addFuzzyInput(deltaError);
  fuzzy->addFuzzyOutput(dayaHeater);

  // Rule base
  // Rule 1: Jika Error Positif AND dError Positif = Besar (Suhu dingin dan makin turun)
  FuzzyRuleAntecedent* ifErrPosAndDePos = new FuzzyRuleAntecedent();
  ifErrPosAndDePos->joinWithAND(errorPositif, DE_Positif);
  FuzzyRuleConsequent* thenBesar1 = new FuzzyRuleConsequent();
  thenBesar1->addOutput(besar);
  fuzzy->addFuzzyRule(new FuzzyRule(1, ifErrPosAndDePos, thenBesar1));

  // Rule 2: Jika Error Positif AND dError Nol = Besar (Suhu dingin dan stabil)
  FuzzyRuleAntecedent* ifErrPosAndDeNol = new FuzzyRuleAntecedent();
  ifErrPosAndDeNol->joinWithAND(errorPositif, DE_Nol);
  FuzzyRuleConsequent* thenBesar2 = new FuzzyRuleConsequent();
  thenBesar2->addOutput(besar);
  fuzzy->addFuzzyRule(new FuzzyRule(2, ifErrPosAndDeNol, thenBesar2));

  // Rule 3: Jika Error Positif AND dError Negatif = Sedang (Suhu dingin tapi mulai naik)
  FuzzyRuleAntecedent* ifErrPosAndDeNeg = new FuzzyRuleAntecedent();
  ifErrPosAndDeNeg->joinWithAND(errorPositif, DE_Negatif);
  FuzzyRuleConsequent* thenSedang1 = new FuzzyRuleConsequent();
  thenSedang1->addOutput(sedang);
  fuzzy->addFuzzyRule(new FuzzyRule(3, ifErrPosAndDeNeg, thenSedang1));

  // Rule 4: Jika Error Nol AND dError Positif = Sedang (Suhu pas tapi mulai turun)
  FuzzyRuleAntecedent* ifErrNolAndDePos = new FuzzyRuleAntecedent();
  ifErrNolAndDePos->joinWithAND(errorNol, DE_Positif);
  FuzzyRuleConsequent* thenSedang2 = new FuzzyRuleConsequent();
  thenSedang2->addOutput(sedang);
  fuzzy->addFuzzyRule(new FuzzyRule(4, ifErrNolAndDePos, thenSedang2));

  // Rule 5: Jika Error Nol AND dError Nol = Kecil (Suhu sudah pas dan stabil)
  FuzzyRuleAntecedent* ifErrNolAndDeNol = new FuzzyRuleAntecedent();
  ifErrNolAndDeNol->joinWithAND(errorNol, DE_Nol);
  FuzzyRuleConsequent* thenSedang3 = new FuzzyRuleConsequent();
  thenSedang3->addOutput(sedang);
  fuzzy->addFuzzyRule(new FuzzyRule(5, ifErrNolAndDeNol, thenSedang3));

  // Rule 6: Jika Error Nol AND dError Negatif = Kecil (Suhu pas tapi ada tren naik)
  FuzzyRuleAntecedent* ifErrNolAndDeNeg = new FuzzyRuleAntecedent();
  ifErrNolAndDeNeg->joinWithAND(errorNol, DE_Negatif);
  FuzzyRuleConsequent* thenKecil1 = new FuzzyRuleConsequent();
  thenKecil1->addOutput(kecil);
  fuzzy->addFuzzyRule(new FuzzyRule(6, ifErrNolAndDeNeg, thenKecil1));

  // Rule 7: Jika Error Negatif AND dError Positif = Kecil (Suhu panas tapi mulai turun)
  FuzzyRuleAntecedent* ifErrNegAndDePos = new FuzzyRuleAntecedent();
  ifErrNegAndDePos->joinWithAND(errorNegatif, DE_Positif);
  FuzzyRuleConsequent* thenKecil2 = new FuzzyRuleConsequent();
  thenKecil2->addOutput(kecil);
  fuzzy->addFuzzyRule(new FuzzyRule(7, ifErrNegAndDePos, thenKecil2));

  // Rule 8: Jika Error Negatif AND dError Nol = Kecil (Suhu panas dan stabil di atas target)
  FuzzyRuleAntecedent* ifErrNegAndDeNol = new FuzzyRuleAntecedent();
  ifErrNegAndDeNol->joinWithAND(errorNegatif, DE_Nol);
  FuzzyRuleConsequent* thenKecil3 = new FuzzyRuleConsequent();
  thenKecil3->addOutput(kecil);
  fuzzy->addFuzzyRule(new FuzzyRule(8, ifErrNegAndDeNol, thenKecil3));

  // Rule 9: Jika Error Negatif AND dError Negatif = Kecil (Suhu panas dan masih terus naik)
  FuzzyRuleAntecedent* ifErrNegAndDeNeg = new FuzzyRuleAntecedent();
  ifErrNegAndDeNeg->joinWithAND(errorNegatif, DE_Negatif);
  FuzzyRuleConsequent* thenKecil4 = new FuzzyRuleConsequent();
  thenKecil4->addOutput(kecil);
  fuzzy->addFuzzyRule(new FuzzyRule(9, ifErrNegAndDeNeg, thenKecil4));

  delay(5000); 
}

void loop() {
  sensors.requestTemperatures(); 
  float temp = sensors.getTempCByIndex(0);

  if (temp <= -100 || temp >= 80) return;

  // Hitung error (Setpoint - Actual)
  float errorVal = setpoint - temp;
  // Hitung delta error (Error saat ini - Error sebelumnya)
  float dErrorVal = errorVal - lastError;

  fuzzy->setInput(1, errorVal);
  fuzzy->setInput(2, dErrorVal);
  fuzzy->fuzzify();

  float output = fuzzy->defuzzify(1);

  // Kontrol Dimmer (0-100%)
  acDimmer.setPower((int)output);

  unsigned long detik = millis() / 1000;
  Serial.print("Waktu(s): "); Serial.print(detik);
  Serial.print(" | Suhu: "); Serial.print(temp);
  Serial.print(" | Err: "); Serial.print(errorVal);
  Serial.print(" | dErr: "); Serial.print(dErrorVal);
  Serial.print(" | Out Dimmer: "); Serial.println(output);

  lastError = errorVal;
  delay(10000); 
}