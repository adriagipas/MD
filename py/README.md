# Mòdul Python per a depurar

Aquest mòdul fa una implementació bàsica del simulador utilitzant
Python, SDL1.2 i OpenGL amb l'objectiu de poder depurar el simulador:

- Pantalla amb resolució original
- Controls "hardcodejats":
  - W: Amunt
  - S: Avall
  - A: Esquerra
  - D: Dreta
  - I: A
  - O: B
  - P: C
  - START: Espai
- No es desa l'estat
- **CTRL-Q** per a eixir.
- **CTRL-R** per a reset.

Per a instal·lar el mòdul

```
pip install .
```

Un exemple bàsic d'ús es pot trobar en **exemple.py**:
```
python3 exemple.py ROM.md
```

En la carpeta **debug** hi ha un script utilitzat per a depurar.
