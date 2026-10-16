# Histórico de alterações

## 0.0.2a (alfa)
- Adiciona o clamp de contagem de dispositivos do `winmm`: `mixerGetNumDevs`, `waveOutGetNumDevs` e `waveInGetNumDevs` nunca informam mais de N dispositivos. N vem do `dsound_proxy.cfg` (padrão 16).
- Mantém o repasse do DirectSound e o log `dsound_proxy.log`.
- Verificado em apenas uma máquina: a contagem do mixer era 28 e foi limitada a 16, e o jogo chegou ao menu.

## 0.0.1a (interna, não publicada)
- Proxy de DirectSound apenas com log, usado para diagnosticar o problema. Mostrou que o jogo nunca enumera dispositivos DirectSound e cria apenas o padrão.
