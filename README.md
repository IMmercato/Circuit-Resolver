# Circuit Resolver

Circuit Resolver is a C-based command-line tool designed to parse, render, and evaluate simple electrical circuits represented as text strings. The program supports circuits with series and parallel configurations, and it can even attempt to calculate the value of an unknown resistance (denoted by “x”) under specific conditions. Please note that while the code provides a robust foundation for analysis, the calculation of the unknown resistance Rx in more complex circuits still requires further refinement.

## Table of Contents

- [Overview](#overview)
- [Features](#features)
- [How It Works](#how-it-works)
- [Usage Instructions](#usage-instructions)
- [Known Limitations](#known-limitations)
- [Credits](#credits)

## Overview

This project implements a circuit resolver that:
- **Parses a circuit string:** The circuit is provided as a string input with tokens representing circuit elements.
- **Renders a visual representation:** The circuit is drawn on a grid using various UTF-16 (wide character) symbols.
- **Evaluates the circuit:** It computes the equivalent resistance of circuits by summing resistors in series and applying formulas for parallel circuits.
- **Handles unknown resistances:** Special tokens denote unknown resistances. When a single unknown resistor is present in a parallel group, the program attempts to solve for its value using a symbolic flowchart method.

## Features

- **Grid-based rendering:** Uses a 2D array to represent the circuit visually using fixed-width blocks.
- **Parsing logic:** Converts circuit string tokens—such as numbers for known resistors, 'x' for unknown resistors, and connectors for series or parallel configurations—into a linked list of circuit blocks.
- **Series and Parallel Analysis:** Functions dedicated to adding resistors in series and combining parallel branches.
- **Interactive Input:** The program prompts the user for both the circuit design and additional measured parameters (like overall resistance, current, and voltage).
- **Custom Circuit Drawing:** Simple circuits (i.e., circuits without parallel groups or unknown resistances) are rendered with a custom drawing style.

## How It Works

1. **Input Parsing:**
   - The circuit must start with a `+` symbol to denote the generator and end with a `-` to mark circuit closure.
   - Numerical tokens represent resistor values, while the letter `x` indicates an unknown resistance.
   - Special tokens (`_`, `•`, `||`, `=`, etc.) define connections in series and nodes for parallel groups.

2. **Block Creation:**
   - A linked list is dynamically built where each node (or block) represents an element of the circuit.
   - Block types include known resistors, connections, bends, and markers for the start or end of parallel groups.

3. **Rendering the Circuit:**
   - A two-dimensional grid is initialized with spaces.
   - Each block is drawn on the grid at a specific row (calculated using a “main” row and shift based on parallel depth) and column position.
   - After all blocks are rendered, the circuit is visually “closed” by drawing vertical conductors and a closing horizontal line.
   - Finally, a generator symbol centered below the circuit completes the visual representation.

4. **Resistance Evaluation:**
   - For circuits made entirely of known resistors, the overall (equivalent) resistance is computed by simply summing or combining series/parallel values accordingly.
   - If an unknown resistor is detected in a parallel group, the program attempts to extract the symbolic structure and solve the following equation:
     \[
     R_{x} = \frac{R_{\text{par}} \cdot S_k}{S_k - R_{\text{par}}} - S_u
     \]
     where:
     - \(S_0\) and \(S_3\) are the sums of series resistances before and after the parallel group, respectively.
     - \(S_u\) is the sum of the branch with the unknown resistor.
     - \(S_k\) is the sum (or value) of the complete branch.
     - \(R_{\text{par}} = R_{\text{measured}} - (S_0 + S_3)\)
     
     The unknown resistor is then solved, provided the structure is correctly detected. Otherwise, a simpler subtraction is used if a resistor appears in series alone. 

5. **Additional Calculations:**
   - Should the user have available values for the circuit’s current \(I\) or voltage \(V\), the program calculates the missing parameter using Ohm’s law.

## Usage Instructions

1. **Compilation:**
   Compile the code using a standard C compiler. For example, if you use `gcc`:
   ```bash
   gcc -o circuit_resolver Circuit.c -Wall -Wextra
   ```

2. **Running the Program:**
   Execute the program from your terminal:
   ```bash
   ./circuit_resolver
   ```
   
3. **Input Format:**
   - Your circuit input must start with a `+` (to indicate the generator) and end with a `-` indicating circuit closure.
   - Example input: `+10_20•x||30=•-`
     - Here `10` and `20` are series resistances.
     - `x` represents an unknown resistance placed within a parallel group.
     - `30` represents a known resistance in a different branch.
     
   Follow on-screen instructions for entering measured resistance, current, and voltage values.

4. **Visual Output:**
   - If the circuit is simple (i.e., no parallel groups or unknown resistor), a custom-drawn circuit appears.
   - For more complex circuits, the full grid-based representation will be printed along with the generator symbol.

5. **Error Handling:**
   - The program validates user input and limits the number of attempts for providing a correct measured resistance value.
   - An error message is displayed if critical parameters are non-positive or inconsistent with the expected circuit design.

## Known Limitations

- **Complex Circuit Analysis:**  
  The algorithm used for calculating the unknown resistance \( R_x \) works only in specific cases (primarily where only one unknown resistor exists within a well-structured parallel group). In circuits with multiple unknowns or non-standard configurations, the Rx calculation may not function as expected.
- **Incomplete Symbolic Handling:**  
  Further work is needed to extend the symbolic analysis method to handle a broader range of circuit complexities and to validate different circuit topologies reliably.

Due to these issues, the code is still under active development and improvement.

## Credits

- **Development Tools and Libraries:**  
  - Standard C libraries, including `<stdio.h>`, `<wchar.h>`, `<locale.h>`, `<stdlib.h>`, `<wctype.h>`, and `<string.h>`, were used for implementing core functionalities.
- **Inspiration and Resources:**
  - This circuit-Resolver idea was get by school ex. where was assigned to program a simple Rx calculater of a simple paralal circuit.
  - The main approach for making the Req calculation was devoloped by using a specific Flowchart from phisics, special thanks for prof. M. Grossi's Flowchart.
  - The approach for parsing and rendering circuits was inspired by similar grid-based rendering techniques employed in educational circuit simulators.

---
