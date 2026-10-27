* TI OPA2992 모델을 opamp_model 핀 순서로 감싼다. vendor/opa2992.lib 필요(README)
.include vendor/opa2992.lib
.subckt opamp_model inp inn out vcc vee
X1 inp inn vcc vee out OPA2992
.ends
