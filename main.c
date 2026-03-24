#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <emscripten.h> 

#define MAX_ACCOUNTS 100
#define ACC_FILE "accounts.dat"
#define LOG_FILE "transactions.txt"

typedef struct {
    char accNo[50]; 
    char name[50];
    double balance;
} Account;

Account accounts[MAX_ACCOUNTS];
int accountCount = 0;
char responseMessage[4096]; 

void loadAccounts() {
    FILE *file = fopen(ACC_FILE, "rb");
    if (file != NULL) {
        fread(&accountCount, sizeof(int), 1, file);
        fread(accounts, sizeof(Account), accountCount, file);
        fclose(file);
    }
}

void saveAccounts() {
    FILE *file = fopen(ACC_FILE, "wb");
    if (file != NULL) {
        fwrite(&accountCount, sizeof(int), 1, file);
        fwrite(accounts, sizeof(Account), accountCount, file);
        fclose(file);
    }
}

void logTransaction(const char* accNo, const char* type, double amount) {
    FILE *file = fopen(LOG_FILE, "a");
    if (file != NULL) {
        time_t now;
        time(&now);
        char *dateStr = ctime(&now);
        dateStr[strlen(dateStr) - 1] = '\0'; 
        // Notice the spacing and cleaner tabular log output structure.
        fprintf(file, "Acc: %s | %-8s | + ₹%8.2lf | %s\n", accNo, type, amount, dateStr);
        fclose(file);
    }
}

int searchAccount(const char* accNo) {
    for (int i = 0; i < accountCount; i++) {
        if (strcmp(accounts[i].accNo, accNo) == 0) return i;
    }
    return -1;
}

EMSCRIPTEN_KEEPALIVE
const char* createAccountWeb(const char* accNo, const char* name, double balance) {
    if (accountCount >= MAX_ACCOUNTS) return "⛔ Error: AK's Bank is at maximum capacity!";
    if (searchAccount(accNo) != -1) return "⛔ Error: Account number already exists in AK's Bank!";
    if (balance < 0) return "⛔ Error: Initial deposit cannot be negative!";

    strcpy(accounts[accountCount].accNo, accNo);
    strcpy(accounts[accountCount].name, name);
    accounts[accountCount].balance = balance;
    accountCount++;
    
    saveAccounts();
    logTransaction(accNo, "Created", balance);
    
    sprintf(responseMessage, "✅ Success! Account %s has been created for %s.", accNo, name);
    return responseMessage;
}

EMSCRIPTEN_KEEPALIVE
const char* depositWeb(const char* accNo, double amount) {
    int index = searchAccount(accNo);
    if (index == -1) return "⛔ Error: Account not found in AK's Bank!";
    if (amount <= 0) return "⛔ Error: Deposit must be > 0!";

    accounts[index].balance += amount;
    saveAccounts();
    logTransaction(accNo, "Deposit", amount);

    sprintf(responseMessage, "💰 Deposited ₹%.2lf into %s.\nNew Balance: ₹%.2lf", amount, accNo, accounts[index].balance);
    return responseMessage;
}

EMSCRIPTEN_KEEPALIVE
const char* withdrawWeb(const char* accNo, double amount) {
    int index = searchAccount(accNo);
    if (index == -1) return "⛔ Error: Account not found in AK's Bank!";
    if (amount <= 0) return "⛔ Error: Amount must be > 0!";
    if (accounts[index].balance < amount) return "⛔ Error: Insufficient funds!";

    accounts[index].balance -= amount;
    saveAccounts();
    logTransaction(accNo, "Withdraw", amount);

    sprintf(responseMessage, "💸 Withdrew ₹%.2lf from %s.\nRemaining: ₹%.2lf", amount, accNo, accounts[index].balance);
    return responseMessage;
}

EMSCRIPTEN_KEEPALIVE
const char* transferWeb(const char* fromAcc, const char* toAcc, double amount) {
    if (strcmp(fromAcc, toAcc) == 0) return "⛔ Error: Cannot wire money to the same account!";
    if (amount <= 0) return "⛔ Error: Transfer amount must be > 0!";

    int fromIndex = searchAccount(fromAcc);
    int toIndex = searchAccount(toAcc);

    if (fromIndex == -1) return "⛔ Error: Sender Account not found!";
    if (toIndex == -1) return "⛔ Error: Receiver Account not found!";

    if (accounts[fromIndex].balance < amount) return "⛔ Error: Insufficient funds for Wire Transfer!";

    accounts[fromIndex].balance -= amount;
    accounts[toIndex].balance += amount;
    
    saveAccounts();
    
    logTransaction(fromAcc, "Wire Out", amount);
    logTransaction(toAcc, "Wire In", amount);

    sprintf(responseMessage, "💸 Wire Transfer Successful!\nSent ₹%.2lf from %s to %s.\nYour New Balance: ₹%.2lf", 
        amount, fromAcc, toAcc, accounts[fromIndex].balance);
    return responseMessage;
}

EMSCRIPTEN_KEEPALIVE
const char* getAccountJSONWeb(const char* accNo) {
    int index = searchAccount(accNo);
    if (index == -1) return "{}";
    sprintf(responseMessage, "{\"accNo\":\"%s\",\"name\":\"%s\",\"balance\":%.2lf}", 
        accounts[index].accNo, accounts[index].name, accounts[index].balance);
    return responseMessage;
}

EMSCRIPTEN_KEEPALIVE
const char* viewAccountWeb(const char* accNo) {
    int index = searchAccount(accNo);
    if (index == -1) return "⛔ Error: Account not found in AK's Bank!";

    sprintf(responseMessage, "--- AK's Bank Summary ---\nAcc No: %s\nHolder: %s\nBalance: ₹%.2lf\n-------------------------", 
        accounts[index].accNo, accounts[index].name, accounts[index].balance);
    return responseMessage;
}

EMSCRIPTEN_KEEPALIVE
const char* viewLogsWeb(const char* accNo) {
    if (searchAccount(accNo) == -1) return "⛔ Error: Account not found in AK's Bank!";
    
    FILE *file = fopen(LOG_FILE, "r");
    if (!file) return "No transactions found.";

    char line[512];
    char history[5][512];
    int count = 0;
    char searchStr[100];
    sprintf(searchStr, "Acc: %s ", accNo); // Has to match exact format

    while (fgets(line, sizeof(line), file)) {
        if (strstr(line, searchStr) != NULL) {
            strcpy(history[count % 5], line); 
            count++;
        }
    }
    fclose(file);

    if (count == 0) return "No historic records found.";
    
    responseMessage[0] = '\0'; 
    int start = (count > 5) ? (count % 5) : 0;
    int limit = (count > 5) ? 5 : count;
    
    strcat(responseMessage, "📋 AK's Bank - Account Audit Logs:\n\n");
    for (int i = 0; i < limit; i++) {
        strcat(responseMessage, history[(start + i) % 5]);
    }
    return responseMessage;
}

int main() {
    loadAccounts(); 
    return 0;
}
