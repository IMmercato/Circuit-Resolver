#include <stdio.h>
#include <wchar.h>
#include <locale.h>
#include <stdlib.h>
#include <wctype.h>
#include <string.h>

#define MAX_LEN 100
#define BLOCK_WIDTH 6   // larghezza fissa di ciascun blocco
#define ROWS 9          // numero di righe della griglia
#define COLS 100        // numero massimo di blocchi (colonne)
#define MAIN_ROW 4      // riga “centrale” per il circuito

// Blocchi grafici per rappresentare il circuito
const wchar_t *RES_BLOCK     = L"\\/\\/\\";
const wchar_t *RX_BLOCK      = L"\\/Rx\\/";  // blocco grafico per resistenza incognita
const wchar_t *CONN_BLOCK    = L"------";
const wchar_t *BEND_BLOCK    = L"|-----";
const wchar_t *NODE_START     = L"----*|";
const wchar_t *NODE_END       = L"|*----";
const wchar_t *NODE_BEND      = L"|*---|";
const wchar_t *PAR_START      = L"|-----";
const wchar_t *PAR_END        = L"------|";
const wchar_t *UP_PIPE        = L"|";
const wchar_t *EMPTY_BLOCK    = L"      ";
const wchar_t *GEN_BLOCK      = L"---| '---";

// Tipologia dei blocchi (per disegno e calcolo)
typedef enum {
    BLOCK_RES,         // Resistenza nota
    BLOCK_CONN,        // Connessione in serie
    BLOCK_BEND,        // Bender/curva
    BLOCK_NODE_START,  // Apertura gruppo parallelo
    BLOCK_NODE_END,    // Chiusura gruppo parallelo
    BLOCK_NODE_BEND,   // Simbolo '-' che chiude il circuito
    BLOCK_PAR_START,   // Inizio ramo parallelo (usa depth=-1)
    BLOCK_PAR_END,     // Fine ramo parallelo
    BLOCK_UP_PIPE,     // Collegamento verticale dal generatore
    BLOCK_UP_RES_PIPE  // Primo elemento dopo il generatore
} BlockType;

typedef struct Block {
    BlockType type;
    int depth;       // 0 per circuito principale, valori negativi per rami paralleli
    int col;         // posizione orizzontale (sequenziale)
    int is_unknown;  // 1 se il blocco rappresenta una resistenza incognita (token 'x')
    double value;    // valore della resistenza (se noto); per "x" viene memorizzato -1
    struct Block *next;
} Block;

wchar_t grid[ROWS][COLS * BLOCK_WIDTH];
Block *head = NULL, *tail = NULL;
int nodeOpen = 0; // Per alternare apertura/chiusura del gruppo parallelo

/* Prototipi di valutazione */
double evaluateSeries(Block **p);
double evaluateParallelGroup(Block **p);

/* --- FUNZIONI DI UTILITÀ PER IL DISPOSITIVO A GRIGLIA --- */
// Aggiunge un blocco alla lista collegata
void add_block(BlockType type, int col, int depth, int is_unknown, double value) {
    Block *b = malloc(sizeof(Block));
    if(!b) {
        wprintf(L"Errore di allocazione!\n");
        exit(1);
    }
    b->type = type;
    b->col = col;
    b->depth = depth;
    b->is_unknown = is_unknown;
    b->value = value;
    b->next = NULL;
    if (!head)
        head = tail = b;
    else {
        tail->next = b;
        tail = b;
    }
}

// Inizializza la griglia (riempie tutto di spazi)
void init_grid() {
    for (int r = 0; r < ROWS; r++)
        for (int c = 0; c < COLS * BLOCK_WIDTH; c++)
            grid[r][c] = L' ';
}

// Inserisce un blocco grafico nella griglia alla data riga e colonna
void draw_block(const wchar_t *block, int row, int col) {
    int offset = col * BLOCK_WIDTH;
    for (int i = 0; block[i] != L'\0'; i++) {
        if (offset + i < COLS * BLOCK_WIDTH)
            grid[row][offset + i] = block[i];
    }
}

// Stampa la griglia fino a "max_col" blocchi
void print_grid(int max_col) {
    for (int r = 0; r < ROWS; r++) {
        grid[r][max_col * BLOCK_WIDTH] = L'\0';
        wprintf(L"%ls\n", grid[r]);
    }
}

// Disegna la chiusura del circuito: due conduttori verticali e una linea orizzontale in fondo
void close_circuit(int max_col) {
    int start_x = BLOCK_WIDTH / 2;
    int end_x   = (max_col - 1) * BLOCK_WIDTH + BLOCK_WIDTH / 2;
    for (int r = MAIN_ROW; r < ROWS; r++)
        grid[r][start_x] = L'|';
    for (int r = MAIN_ROW; r < ROWS; r++)
        grid[r][end_x] = L'|';
    for (int x = start_x; x <= end_x; x++)
        grid[ROWS - 1][x] = L'-';
}

/* --- FUNZIONE DI PARSING --- */
void parse_circuit(const wchar_t *circuit) {
    int depth = 0;
    int col = 0;
    size_t len = wcslen(circuit);
    nodeOpen = 0;
    int first = 1;

    for (size_t i = 0; i < len; i++) {
        wchar_t token = circuit[i];
        if(token == L'+')
            continue;  // il "+" indica il generatore, non genera blocchi

        if(iswdigit(token) || token == L'x') {
            int is_unknown = 0;
            double val = 0;
            if(token == L'x') {
                is_unknown = 1;
                val = -1;
                i++; // consuma 'x'
            } else {
                wchar_t number[20] = {0};
                int k = 0;
                int has_decimal = 0;
                while(i < len && (iswdigit(circuit[i]) || circuit[i] == L'.' || circuit[i] == L',')) {
                    if(circuit[i] == L'.' || circuit[i] == L',') {
                        if(!has_decimal) {
                            number[k++] = L'.'; // forza il punto come separatore
                            has_decimal = 1;
                        }
                        i++;
                        continue;
                    }
                    number[k++] = circuit[i++];
                }
                i--; // aggiusta l'indice
                val = wcstod(number, NULL);
            }
            if(first) {
                add_block(BLOCK_UP_RES_PIPE, col++, depth, is_unknown, val);
                first = 0;
            } else {
                add_block(BLOCK_RES, col++, depth, is_unknown, val);
            }
            continue;
        }
        switch(token) {
            case L'_':
                add_block(BLOCK_CONN, col++, depth, 0, 0);
                break;
            case L'-':
                // Il "-" chiude il circuito
                add_block(BLOCK_NODE_BEND, col++, depth, 0, 0);
                break;
            case L'*':
                // Alterna apertura/chiusura gruppo parallelo
                if(nodeOpen == 0) {
                    add_block(BLOCK_NODE_START, col++, depth, 0, 0);
                    nodeOpen = 1;
                    first = 1; // resetta per il gruppo parallelo
                } else {
                    add_block(BLOCK_NODE_END, col++, depth, 0, 0);
                    nodeOpen = 0;
                }
                break;
            case L'|':
                if(i+1 < len && circuit[i+1] == L'|') {
                    depth = -1;
                    i++;
                    add_block(BLOCK_PAR_START, col++, depth, 0, 0);
                    first = 1; // resetta per il ramo parallelo
                } else {
                    add_block(BLOCK_BEND, col++, depth, 0, 0);
                }
                break;
            case L'=':
                add_block(BLOCK_PAR_END, col++, depth, 0, 0);
                depth = 0;
                break;
            default:
                break;
        }
    }
    // Se il primo blocco non è il collegamento al generatore, lo inseriamo in testa
    if(head && head->type != BLOCK_UP_RES_PIPE) {
        Block *pipe = malloc(sizeof(Block));
        pipe->type = BLOCK_UP_RES_PIPE;
        pipe->col = 0;
        pipe->depth = 0;
        pipe->is_unknown = 0;
        pipe->value = 0;
        pipe->next = head;
        head = pipe;
    }
}

/* --- FUNZIONI DI RENDERING --- */
// Se un blocco di tipo resistenza ha is_unknown true, usa RX_BLOCK
void render_blocks() {
    Block *curr = head;
    while(curr) {
        int row = MAIN_ROW + curr->depth;
        switch(curr->type) {
            case BLOCK_RES:
            case BLOCK_UP_RES_PIPE:
                if(curr->is_unknown)
                    draw_block(RX_BLOCK, row, curr->col);
                else
                    draw_block(RES_BLOCK, row, curr->col);
                break;
            case BLOCK_CONN:
                draw_block(CONN_BLOCK, row, curr->col);
                break;
            case BLOCK_BEND:
                draw_block(BEND_BLOCK, row, curr->col);
                break;
            case BLOCK_NODE_START:
                draw_block(NODE_START, row, curr->col);
                break;
            case BLOCK_NODE_END:
                draw_block(NODE_END, row, curr->col);
                break;
            case BLOCK_NODE_BEND:
                draw_block(NODE_BEND, row, curr->col);
                break;
            case BLOCK_PAR_START:
                draw_block(PAR_START, row, curr->col);
                break;
            case BLOCK_PAR_END:
                draw_block(PAR_END, row, curr->col);
                break;
            case BLOCK_UP_PIPE:
                draw_block(UP_PIPE, MAIN_ROW - 1, curr->col);
                break;
        }
        curr = curr->next;
    }
}

// Disegna il generatore centrato sotto il circuito
void draw_generator(int max_col) {
    int gen_len = wcslen(GEN_BLOCK);
    int total_width = max_col * BLOCK_WIDTH;
    int pad = (total_width - gen_len) / 2;
    for (int i = 0; i < pad; i++)
        wprintf(L" ");
    wprintf(L"%ls\n", GEN_BLOCK);
}

/* --- FUNZIONI DI CALCOLO DELLA RESISTENZA EQUVALENTE --- */
// Le funzioni di calcolo ignorano le resistenze incognite (is_unknown == 1)
double evaluateSeries(Block **p) {
    double total = 0.0;
    while(*p) {
        Block *curr = *p;
        if ((curr->type == BLOCK_RES || curr->type == BLOCK_UP_RES_PIPE) && !curr->is_unknown) {
            total += curr->value;
            *p = curr->next;
        }
        else if (curr->type == BLOCK_NODE_START) {
            *p = curr->next;
            total += evaluateParallelGroup(p);
        }
        else if (curr->type == BLOCK_NODE_END ||
                 curr->type == BLOCK_PAR_START ||
                 curr->type == BLOCK_PAR_END) {
            break;
        }
        else {
            *p = curr->next;
        }
    }
    return total;
}

double evaluateParallelGroup(Block **p) {
    double branches[10] = {0};
    int count = 0;
    
    // Primo ramo
    branches[count++] = evaluateSeries(p);
    
    // Eventuali rami separati da BLOCK_PAR_START ... BLOCK_PAR_END
    while(*p && (*p)->type == BLOCK_PAR_START) {
        *p = (*p)->next; // consuma separatore
        branches[count++] = evaluateSeries(p);
        if(*p && (*p)->type == BLOCK_PAR_END)
            *p = (*p)->next;
        else
            break;
    }
    if(*p && (*p)->type == BLOCK_NODE_END)
        *p = (*p)->next;
    
    double invSum = 0.0;
    for(int i = 0; i < count; i++){
        if(branches[i] > 0)
            invSum += 1.0 / branches[i];
    }
    double parallelValue = (invSum > 0) ? (1.0 / invSum) : 0.0;
    return parallelValue;
}

double calculate_total_resistance_new() {
    Block *p = head;
    return evaluateSeries(&p);
}

/* --- FUNZIONI PER LA GESTIONE DELLE INCOGNITE --- */
// Conta il numero di resistenze incognite
int count_unknowns() {
    int count = 0;
    for(Block *cur = head; cur; cur = cur->next) {
        if ((cur->type == BLOCK_RES || cur->type == BLOCK_UP_RES_PIPE) && cur->is_unknown)
            count++;
    }
    return count;
}

// --- FUNZIONI DI CALCOLO SIMBOLICO (per incognite in gruppi paralleli) ---
// Questa parte applica il flowchart per circuiti series-parallel dove l'incognita appare in un gruppo parallelo.
// La struttura attesa è:
//   + S0 _ [ parallel group: BLOCK_NODE_START, branch1: (S_u + X) ; eventualmente separato da BLOCK_PAR_START,
//             branch2: S_k (completa, nota), e poi BLOCK_PAR_END, BLOCK_NODE_END ] _ S3 -
// Dove:
//   S0: somma dei resistori in serie prima del gruppo parallelo,
//   S3: somma dei resistori in serie dopo il gruppo parallelo,
//   S_u: somma dei resistori noti nella branch contenente l'incognita,
//   S_k: somma (o valore univoco) nella branch completamente nota.
//
// L'equazione è:
//   Req_measured = S0 + ( (S_u + X)*S_k/(S_u + X + S_k) ) + S3
// Da cui risolvendo per X si ottiene:
//   X = (R_par * S_k)/(S_k - R_par) - S_u,    dove R_par = Req_measured - (S0 + S3)
// Se la struttura non viene rilevata, la funzione restituisce 0.
int extract_parallel_structure(double *pS0, double *pS3, double *pSu, double *pSk) {
    Block *cur = head;
    double S0 = 0, S3 = 0;
    // S0: somma dei resistori in serie prima del primo BLOCK_NODE_START
    while(cur && cur->type != BLOCK_NODE_START) {
        if ((cur->type == BLOCK_RES || cur->type == BLOCK_UP_RES_PIPE) && !cur->is_unknown)
            S0 += cur->value;
        cur = cur->next;
    }
    if(!cur) return 0; // nessun gruppo parallelo trovato

    // Il gruppo parallelo inizia con BLOCK_NODE_START
    Block *group_start = cur; // BLOCK_NODE_START
    cur = cur->next;
    // Consideriamo l'ipotesi con due branche
    double branch1 = 0;
    int branch1_unknown = 0;
    while(cur && cur->type != BLOCK_PAR_START && cur->type != BLOCK_NODE_END) {
        if(cur->type == BLOCK_RES || cur->type == BLOCK_UP_RES_PIPE) {
            if(cur->is_unknown)
                branch1_unknown = 1;
            else
                branch1 += cur->value;
        }
        cur = cur->next;
    }
    double branch2 = 0;
    int branch2_unknown = 0;
    if(cur && cur->type == BLOCK_PAR_START) {
        cur = cur->next;
        while(cur && cur->type != BLOCK_NODE_END && cur->type != BLOCK_PAR_END) {
            if(cur->type == BLOCK_RES || cur->type == BLOCK_UP_RES_PIPE) {
                if(cur->is_unknown)
                    branch2_unknown = 1;
                else
                    branch2 += cur->value;
            }
            cur = cur->next;
        }
        if(cur && cur->type == BLOCK_PAR_END)
            cur = cur->next;
    }
    if(cur && cur->type == BLOCK_NODE_END)
        cur = cur->next;
    while(cur) {
        if ((cur->type == BLOCK_RES || cur->type == BLOCK_UP_RES_PIPE) && !cur->is_unknown)
            S3 += cur->value;
        cur = cur->next;
    }

    if(branch1_unknown && !branch2_unknown) {
        *pSu = branch1;
        *pSk = branch2;
    } else if(branch2_unknown && !branch1_unknown) {
        *pSu = branch2;
        *pSk = branch1;
    } else {
        return 0;
    }
    *pS0 = S0;
    *pS3 = S3;
    return 1;
}
// Fine area funzioni simboliche

/* --- FUNZIONI DI DISEGNO "CUSTOM" PER CIRCUITI SEMPLICI --- */
// Se il circuito non contiene gruppi paralleli né incognite, lo consideriamo semplice.
int isSimpleCircuit() {
    Block *cur = head;
    while(cur) {
         if(cur->type == BLOCK_NODE_START || cur->type == BLOCK_NODE_END ||
            cur->type == BLOCK_PAR_START || cur->type == BLOCK_PAR_END ||
            ((cur->type == BLOCK_RES || cur->type == BLOCK_UP_RES_PIPE) && cur->is_unknown))
              return 0;
         cur = cur->next;
    }
    return 1;
}

void customDrawSimpleCircuit() {
    wprintf(L"\n=== Disegno del circuito ===\n\n");
    wprintf(L"|-----%ls-----|\n", RES_BLOCK);
    wprintf(L"|               |\n");
    wprintf(L"|------| '------|\n");
}

/* --- ISTRUZIONI --- */
void print_instructions() {
    wprintf(L"\n=== Istruzioni ===\n"
            L"+       -> Generatore (inizio circuito)\n"
            L"Numero  -> Resistenza (es: 10, 47, ecc.)\n"
            L"x       -> Resistenza incognita da trovare\n"
            L"_       -> Connessione in serie\n"
            L"*       -> Nodo (inizio/fine gruppo parallelo)\n"
            L"||      -> Inizio ramo parallelo (separa i rami)\n"
            L"=       -> Fine ramo parallelo\n"
            L"-       -> Chiusura circuito\n"
            L"\nEsempio valido: +10_20*x||30=*-\n");
}

int main() {
    setlocale(LC_ALL, "");
    wchar_t circuit[MAX_LEN];
    double I = -1, V = -1;
    int resolved = 0; // Visibile in tutto main

    head = NULL;
    tail = NULL;

    wprintf(L"+++ Circuit Resolver con gestione delle incognite (flowchart) +++\n");
    print_instructions();
    wprintf(L"Inserisci circuito: ");

    if (!fgetws(circuit, MAX_LEN, stdin)) {
        wprintf(L"Errore di lettura del circuito.\n");
        return 1;
    }
    circuit[wcslen(circuit) - 1] = (circuit[wcslen(circuit) - 1] == L'\n') ? L'\0' : circuit[wcslen(circuit) - 1];

    size_t len = wcslen(circuit);
    if(len < 2 || circuit[0] != L'+' || circuit[len - 1] != L'-') {
        wprintf(L"Errore: il circuito deve iniziare con '+' e terminare con '-'.\n");
        return 1;
    }

    parse_circuit(circuit);

    if(isSimpleCircuit()) {
        customDrawSimpleCircuit();
    } else {
        init_grid();
        render_blocks();

        int max_col = 0;
        for(Block *b = head; b; b = b->next)
            if(b->col > max_col)
                max_col = b->col;
        max_col++;

        close_circuit(max_col);

        wprintf(L"\n=== Disegno del circuito ===\n\n");
        print_grid(max_col);
        draw_generator(max_col);
    }

    double Req_known = calculate_total_resistance_new();
    wprintf(L"\n--- Calcoli ---\n");
    wprintf(L"Somma delle resistenze note (Req_known): %.2f Ohm\n", Req_known);

    int unknown_count = count_unknowns();
    double Req_measured = 0; // Per resistenza equivalente misurata o sum note

    if (unknown_count == 1) {
        double S0, S3, S_u, S_k;
        resolved = extract_parallel_structure(&S0, &S3, &S_u, &S_k);
        if (resolved) {
            wprintf(L"\nIl circuito contiene una resistenza incognita in un gruppo parallelo.\n");
        } else {
            wprintf(L"\nIl circuito contiene una resistenza incognita in serie.\n");
            S0 = S3 = S_u = S_k = 0;
        }

        int valid = 0;
        wchar_t buffer[64];

        for (int attempt = 0; attempt < 3 && !valid; attempt++) {
            wprintf(L"Inserisci il valore complessivo misurato (Req): ");
            fflush(stdout);
            if (fgetws(buffer, sizeof(buffer) / sizeof(wchar_t), stdin) != NULL) {
                buffer[wcscspn(buffer, L"\n")] = L'\0';
                Req_measured = wcstod(buffer, NULL);

                if (Req_measured <= 0) {
                    wprintf(L"Errore: il valore misurato deve essere positivo.\n");
                    continue;
                }

                if (resolved) {
                    double R_par_actual = Req_measured - (S0 + S3);
                    if (S_k <= R_par_actual || R_par_actual <= 0) {
                        wprintf(L"Errore: dati non validi (S_k=%.2f, R_par=%.2f).\n", S_k, R_par_actual);
                        continue;
                    }
                    double Rx = (R_par_actual * S_k) / (S_k - R_par_actual) - S_u;
                    if (Rx <= 0) {
                        wprintf(L"Errore: il calcolo di Rx risulta zero o negativo (%.2f Ohm).\n", Rx);
                        continue;
                    }
                    wprintf(L"Resistenza incognita Rx calcolata: %.2f Ohm\n", Rx);
                    valid = 1;
                } else {
                    double Rx = Req_measured - Req_known;
                    if (Rx <= 0) {
                        wprintf(L"Errore: il calcolo di Rx risulta zero o negativo (%.2f Ohm). Controlla i dati!\n", Rx);
                    } else {
                        wprintf(L"Resistenza incognita Rx calcolata: %.2f Ohm\n", Rx);
                        valid = 1;
                    }
                }
            }
        }

        if (!valid) {
            wprintf(L"\nValore non valido dopo 3 tentativi. Impossibile calcolare Rx.\n");
            return 1;
        }
    } else if (unknown_count > 1) {
        wprintf(L"\nIl circuito contiene più di una resistenza incognita. Calcolo non supportato.\n");
        return 1;
    } else {
        // Nessuna incognita, usa la somma nota come valore equivalente
        Req_measured = Req_known;
    }

    // Variabile per resistenza da usare nel calcolo di corrente e tensione
    double Req_for_current = Req_measured;

    wchar_t input[50];
    wprintf(L"\nInserisci corrente I (A), se nota (-1 se non nota): ");
    if(fgetws(input, sizeof(input) / sizeof(wchar_t), stdin)) {
        input[wcscspn(input, L"\n")] = L'\0';
        I = wcstod(input, NULL);
    }
    wprintf(L"Inserisci tensione V (V), se nota (-1 se non nota): ");
    if(fgetws(input, sizeof(input) / sizeof(wchar_t), stdin)) {
        input[wcscspn(input, L"\n")] = L'\0';
        V = wcstod(input, NULL);
    }

    if (Req_for_current <= 0) {
        wprintf(L"Errore: resistenza per calcolo corrente/tensione non valida (<= 0).\n");
        return 1;
    }

    if (I > 0 && V <= 0)
        V = I * Req_for_current;
    else if (V > 0 && I <= 0)
        I = V / Req_for_current;

    wprintf(L"Tensione: %s%.2f V\n", V > 0 ? L"" : L"(non disponibile) ", V > 0 ? V : 0);
    wprintf(L"Corrente: %s%.2f A\n", I > 0 ? L"" : L"(non disponibile) ", I > 0 ? I : 0);

    if (I > 0 && V > 0)
        wprintf(L"\nCalcoli completati con successo.\n");
    else
        wprintf(L"\nNon tutti i dati sono disponibili per completare l'analisi.\n");

    return 0;
}