/******************************************************************************

                            Online C Compiler.
                Code, Compile, Run and Debug C program online.
Write your code in this editor and press "Run" button to compile and execute it.

*******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <signal.h>

#define POOL_SIZE 4   // Tamanho fixo da thread pool (modelo N:1)
#define MAX_QUEUE 100 // Capacidade máxima da fila FCFS

// Estrutura que representa uma requisição de transferência PIX
typedef struct {
    int id;       // Identificador único da requisição
    int origem;   // Conta de origem do dinheiro
    int destino;  // Conta de destino do dinheiro
    double valor; // Valor da transferência
} PixRequest;

// Base de dados compartilhada: vetor contendo o saldo de 5 contas bancárias
double saldos[5] = {1000.0, 1000.0, 1000.0, 1000.0, 1000.0};

// Fila FCFS (First-Come, First-Served) para armazenar requisições excedentes
PixRequest queue[MAX_QUEUE];
int head = 0;   // Aponta para o início da fila (próximo a ser consumido)
int tail = 0;   // Aponta para o fim da fila (próximo a ser inserido)
int count = 0;  // Número atual de elementos na fila

// Mutex locks para garantir exclusão mútua e evitar condições de corrida
pthread_mutex_t mutex_queue;  // Protege o acesso à fila FCFS compartilhada
pthread_mutex_t mutex_saldos; // Protege a seção crítica da base de dados de saldos

// Vetor de identificadores das threads da pool e flag de controle de execução
pthread_t threads[POOL_SIZE];
int running = 1;

// Manipulador de sinal (Signal Handler) para debugging interativo via Ctrl+C (SIGINT)
void debug_handler(int sig) {
    // Seção crítica para leitura segura do estado da fila
    pthread_mutex_lock(&mutex_queue);
    printf("\n\n====== [DEBUG SIGNAL] ESTADO ATUAL DO SISTEMA PIX ======\n");
    printf("  - Requisições aguardando na fila FCFS: %d\n", count);
    for (int i = 0; i < count; i++) {
        int idx = (head + i) % MAX_QUEUE;
        printf("    -> Req #%d: Conta %d -> Conta %d | Valor: R$ %.2f\n", 
               queue[idx].id, queue[idx].origem, queue[idx].destino, queue[idx].valor);
    }
    pthread_mutex_unlock(&mutex_queue);

    // Seção crítica para leitura segura dos saldos atuais
    pthread_mutex_lock(&mutex_saldos);
    printf("  - Saldos atuais na Base de Dados Compartilhada:\n");
    for (int i = 0; i < 5; i++) {
        printf("    -> Conta %d: R$ %.2f\n", i, saldos[i]);
    }
    printf("======================================================\n\n");
}

// Função executada pelas threads trabalhadoras (Workers) da Thread Pool
void* worker(void* arg) {
    while (running) {
        PixRequest req;
        int tem_requisicao = 0;

        // 1. ACESSO À FILA: Seção crítica protegida por mutex para retirar requisição (FCFS)
        pthread_mutex_lock(&mutex_queue);
        if (count > 0) {
            req = queue[head];              // Retira do início da fila
            head = (head + 1) % MAX_QUEUE;  // Atualiza ponteiro circular
            count--;                        // Decrementa contador de itens
            tem_requisicao = 1;
        }
        pthread_mutex_unlock(&mutex_queue);

        // 2. PROCESSAMENTO DA REQUISIÇÃO: Se houver item na fila, processa
        if (tem_requisicao) {
            // SEÇÃO CRÍTICA: Atualização concorrente da base de dados protegida por mutex
            pthread_mutex_lock(&mutex_saldos);
            if (saldos[req.origem] >= req.valor) {
                saldos[req.origem] -= req.valor; // Debita da origem
                saldos[req.destino] += req.valor; // Credita no destino
                printf("[PIX] Sucesso: Req #%d | Conta %d enviou R$ %.2f para Conta %d\n", 
                       req.id, req.origem, req.valor, req.destino);
            } else {
                printf("[PIX] Erro: Req #%d | Saldo insuficiente na Conta %d\n", req.id, req.origem);
            }
            pthread_mutex_unlock(&mutex_saldos);

            // Simula o tempo de processamento coerente da transação
            usleep(15000); // 15 milissegundos
        } else {
            // Se a fila estiver vazia, descansa brevemente para economizar CPU
            usleep(5000);  // 5 milissegundos
        }
    }
    return NULL;
}

// Função para submeter novas requisições de PIX para a fila FCFS
void submeter_pix(int id, int origem, int destino, double valor) {
    pthread_mutex_lock(&mutex_queue);
    if (count < MAX_QUEUE) {
        // Insere a requisição no fim da fila FCFS
        queue[tail].id = id;
        queue[tail].origem = origem;
        queue[tail].destino = destino;
        queue[tail].valor = valor;
        tail = (tail + 1) % MAX_QUEUE;
        count++;
        printf("[SISTEMA] Requisição #%d enfileirada (FCFS).\n", id);
    } else {
        printf("[SISTEMA] Fila cheia! Requisição #%d rejeitada.\n", id);
    }
    pthread_mutex_unlock(&mutex_queue);
}

int main() {
    // Configura o sinal SIGINT (Ctrl+C) para acionar a rotina de debugging
    signal(SIGINT, debug_handler);

    // Inicializa os mutexes de sincronização
    pthread_mutex_init(&mutex_queue, NULL);
    pthread_mutex_init(&mutex_saldos, NULL);

    // Criação das threads da Thread Pool (modelo N:1)
    for (int i = 0; i < POOL_SIZE; i++) {
        pthread_create(&threads[i], NULL, worker, NULL);
    }

    printf("=== Simulador PIX Iniciado (Thread Pool + FCFS + Mutex + Sinais) ===\n");
    printf("Dica: Pressione Ctrl+C a qualquer momento para ver o estado atual (Debugging).\n\n");

    // Simula a chegada contínua de requisições de clientes do PIX
    for (int i = 1; i <= 15; i++) {
        submeter_pix(i, i % 5, (i + 2) % 5, 25.0 * i);
        usleep(40000); // Intervalo entre as submissões, 40ms
    }

    // Aguarda o término do processamento das transações pendentes
    sleep(4);

    // Encerra as threads da pool de forma controlada
    running = 0;
    for (int i = 0; i < POOL_SIZE; i++) {
        pthread_join(threads[i], NULL);
    }

    // Destrói os recursos de sincronização alocados
    pthread_mutex_destroy(&mutex_queue);
    pthread_mutex_destroy(&mutex_saldos);

    printf("=== Simulação do Sistema PIX Concluída com Sucesso ===\n");
    return 0;
}