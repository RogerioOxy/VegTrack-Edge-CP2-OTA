
#include <cstdio>
#include <cstring>
#include <cmath>
#include <algorithm>
// BEGIN PURE FUNCTIONS - usados também pelos testes locais.
float meanFive(const int *values) {
  int sum = 0;
  for (int i = 0; i < 5; i++) sum += values[i];
  return sum / 5.0f;
}

bool parseVersion(const char *text, unsigned &major, unsigned &minor) {
  if (!text || !*text) return false;
  unsigned parts[2] = {0, 0};
  for (int part = 0; part < 2; part++) {
    unsigned digits = 0;
    while (*text >= '0' && *text <= '9') {
      if (++digits > 5) return false;
      parts[part] = parts[part] * 10 + (*text++ - '0');
      if (parts[part] > 65535) return false;
    }
    if (digits == 0) return false;
    if (part == 0 && *text++ != '.') return false;
  }
  if (*text != '\0') return false;
  major = parts[0];
  minor = parts[1];
  return true;
}

bool isNewer(unsigned major, unsigned minor, unsigned installedMajor,
             unsigned installedMinor) {
  return major > installedMajor ||
         (major == installedMajor && minor > installedMinor);
}
// END PURE FUNCTIONS

// BEGIN V2 PURE FUNCTIONS - a produção e o diagnóstico chamam estas funções.
enum State { NORMAL, ALERTA };

void sortFive(const int *source, int *sorted) {
  for (int i = 0; i < 5; i++) sorted[i] = source[i];
  // Ordenação por inserção de uma cópia, preservando a ordem original.
  for (int i = 1; i < 5; i++) {
    int value = sorted[i];
    int j = i - 1;
    while (j >= 0 && sorted[j] > value) {
      sorted[j + 1] = sorted[j];
      j--;
    }
    sorted[j + 1] = value;
  }
}

int medianFive(const int *source, int *sorted) {
  sortFive(source, sorted);
  return sorted[2]; // terceiro elemento, pois os índices começam em zero
}

State nextState(int median, State prior) {
  if (median >= 16) return ALERTA;
  if (median <= 14) return NORMAL;
  return prior; // 14 < mediana < 16: mantém o estado anterior
}
// END V2 PURE FUNCTIONS

int failures=0, checks=0;
void check(bool ok, const char *name) {
  checks++;
  if (!ok) { failures++; std::printf("FAIL: %s\n",name); }
}
int main() {
  unsigned major=0, minor=0;
  const char *invalid[]={"", "2", ".0", "2.", "x.0", "2.x", "2.0oops", "2.0.1", "-1.0", "65536.0", "2.65536", " 2.0", "2.0 ", "123456.1"};
  for(auto v: invalid) check(!parseVersion(v,major,minor),v);
  check(parseVersion("2.0",major,minor) && major==2 && minor==0,"valid version");
  check(isNewer(2,0,1,0),"upgrade 1.0 -> 2.0");
  check(!isNewer(2,0,2,0),"already current");
  check(!isNewer(1,99,2,0),"downgrade rejected");
  check(isNewer(1,100,1,99),"numeric minor comparison");
  const int example[]={18,12,15,14,20};
  int sorted[5];
  check(medianFive(example,sorted)==15,"assignment median");
  check(std::fabs(meanFive(example)-15.8f)<0.001f,"assignment mean");
  State prior=NORMAL;
  const int medians[]={16,15,14,15};
  const State expectedStates[]={ALERTA,ALERTA,NORMAL,NORMAL};
  for(int i=0;i<4;i++) {
    prior=nextState(medians[i],prior);
    check(prior==expectedStates[i],"sequential boundary");
  }
  check(nextState(15,NORMAL)==NORMAL,"15 holds NORMAL");
  check(nextState(15,ALERTA)==ALERTA,"15 holds ALERTA");
  check(nextState(14,ALERTA)==NORMAL,"14 returns NORMAL");
  check(nextState(16,NORMAL)==ALERTA,"16 enters ALERTA");
  // Todos os 11^5 vetores possíveis das leituras inteiras de 10 a 20.
  // O oráculo usa std::sort; o firmware usa inserção.
  int cases=0;
  for(int a=10;a<=20;a++)for(int b=10;b<=20;b++)for(int c=10;c<=20;c++)
  for(int d=10;d<=20;d++)for(int e=10;e<=20;e++) {
    int source[]={a,b,c,d,e}, before[5], expected[5], result[5];
    std::memcpy(before,source,sizeof source);
    std::memcpy(expected,source,sizeof source);
    std::sort(expected,expected+5);
    int median=medianFive(source,result);
    check(!std::memcmp(source,before,sizeof source),"original preserved");
    check(!std::memcmp(expected,result,sizeof source),"insertion sort");
    check(median==expected[2],"median is third");
    check(std::fabs(meanFive(source)-(a+b+c+d+e)/5.0f)<0.001f,"mean");
    check(nextState(median,NORMAL)==(median>=16?ALERTA:NORMAL),"state from NORMAL");
    check(nextState(median,ALERTA)==(median<=14?NORMAL:ALERTA),"state from ALERTA");
    cases++;
  }
  std::printf("Actual extracted firmware functions: %d vectors, %d checks, %d failures\n",cases,checks,failures);
  std::puts("Hardware, network, JSON library, LEDs and OTA reboot: NOT exercised by host test.");
  return failures?1:0;
}
