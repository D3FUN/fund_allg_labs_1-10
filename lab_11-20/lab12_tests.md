# Лабораторная 12 — что вводить

Все команды выполняются из папки `lab_11-20`.

```sh
make lab12
```

Формат: `./lab12 <входной файл> <-a|-d|/a|/d> <выходной файл>`.

## Корректные тесты

Готовый входной файл `lab12.txt` содержит:

```text
8 Bob Smith 10
3 Ann Smith 10
2 Ann Smith 10
1 Zoe Adams 10
9 A Z 2
```

Ввести в терминале:

```sh
./lab12 lab12.txt -a result.txt
cat result.txt
```

Ожидаемый результат:

```text
9 A Z 2
1 Zoe Adams 10
2 Ann Smith 10
3 Ann Smith 10
8 Bob Smith 10
```

По убыванию:

```sh
./lab12 lab12.txt /d result.txt
cat result.txt
```

Строки должны идти в обратном порядке. `result.txt` создаётся или перезаписывается.

## Ошибочные тесты

```sh
./lab12 missing_file.txt -a result.txt
./lab12 lab12.txt -z result.txt
./lab12 lab12.txt -a lab12.txt
```

Должна появиться ошибка; входной файл нельзя использовать как выходной.
