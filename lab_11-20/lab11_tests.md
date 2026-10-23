# Лабораторная 11 — что вводить

Все команды выполняются из папки `lab_11-20`.

```sh
make lab11
```

Формат: `./lab11 <флаг> <строка>`. Для `-c`: `./lab11 -c <строка> <seed> [ещё строки...]`.

## Корректные тесты

```sh
./lab11 -l hello
# 5
./lab11 -r abcdef
# fedcba
./lab11 -u abcdef
# aBcDeF
./lab11 -n '!b2 a1?Z'
# 21baZ! ?
./lab11 -c first 42 second third
# Все три строки объединятся в псевдослучайном порядке без пробелов.
./lab11 -l ''
# 0
```

## Ошибочные тесты

```sh
./lab11 -z hello
./lab11 -l hello extra
./lab11 -c first -1 second
./lab11 -c first abc second
```

В этих случаях программа должна вывести ошибку.
