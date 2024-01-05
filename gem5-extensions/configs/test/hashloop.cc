#include <iostream>
#include <cstdlib>
#include <vector>
#include <string>
#include <random>
#include <algorithm>

#define RAND_SEED 85354712
auto rng = std::default_random_engine(RAND_SEED);

std::string forenames[100] = {
    "Maria", "Nushi", "Mohammed", "Jose", "Muhammad", "Mohamed", "Wei", "Mohammad", "Ahmed",
    "Yan","Ali", "John", "David", "Li", "Abdul", "Ana", "Ying", "Michael", "Juan", "Anna", "Mary",
    "Jean", "Robert", "Daniel", "Luis", "Carlos", "James", "Antonio", "Joseph", "Hui", "Elena",
    "Francisco", "Hong", "Marie", "Min", "Lei", "Yu", "Ibrahim", "Peter", "Fatima", "Aleksandr",
    "Richard", "Xin", "Bin", "Paul", "Ping", "Lin", "Olga", "Sri", "Pedro", "William", "Rosa",
    "Thomas", "Jorge", "Yong", "Elizabeth", "Sergey", "Ram", "Patricia", "Hassan", "Anita",
    "Manuel", "Victor", "Sandra", "Ming", "Siti", "Miguel", "Emmanuel", "Samuel", "Ling", "Charles",
    "Sarah", "Mario", "Joao", "Tatyana", "Mark", "Rita", "Martin", "Svetlana", "Patrick", "Natalya",
    "Qing", "Ahmad", "Martha", "Andrey", "Sunita", "Andrea", "Christine", "Irina", "Laura", "Linda",
    "Marina", "Carmen", "Ghulam", "Vladimir", "Barbara", "Angela", "George", "Roberto", "Pen"
};

std::string surnames[100] = {
    "Smith", "Johnson", "Williams", "Brown", "Jones", "Garcia", "Miller", "Davis", "Rodriguez",
    "Martinez", "Hernandez", "Lopez", "Gonzalez", "Wilson", "Anderson", "Thomas", "Taylor",
    "Moore", "Jackson", "Martin", "Lee", "Perez", "Thompson", "White", "Harris", "Sanchez",
    "Clark", "Ramirez", "Lewis", "Robinson", "Walker", "Young", "Allen", "King", "Wright",
    "Scott", "Torres", "Nguyen", "Hill", "Flores", "Green", "Adams", "Nelson", "Baker", "Hall",
    "Rivera", "Campbell", "Mitchell", "Carter", "Roberts", "Gomez", "Phillips", "Evans", "Turner",
    "Diaz", "Parker", "Cruz", "Edwards", "Collins", "Reyes", "Stewart", "Morris", "Morales",
    "Murphy", "Cook", "Rogers", "Gutierrez", "Ortiz", "Morgan", "Cooper", "Peterson", "Bailey",
    "Reed", "Kelly", "Howard", "Ramos", "Kim", "Cox", "Ward", "Richardson", "Watson", "Brooks",
    "Chavez", "Wood", "James", "Bennett", "Gray", "Mendoza", "Ruiz", "Hughs", "Price", "Alvarez",
    "Castillo", "Sanders", "Patel", "Myers", "Long", "Ross", "Foster", "Jimenez"
};

std::vector<std::string> all_names;

// djb2 hash function - http://www.cse.yorku.ca/~oz/hash.html
unsigned long hash_djb2(std::string s) {
    unsigned long hash = 5381;
    for (char c : s)
        hash = hash * 33 ^ c;
    return hash;
}

// Generate all combinations of names
void generate_names() {
    for (std::string forename : forenames) {
        for (std::string surname: surnames) {
            all_names.push_back(forename + " " + surname);
        }
    }
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Usage: hashloop BUF_SIZE\n");
        return 1;
    }

    int buf_size = std::stoi(std::string(argv[1]));
    generate_names();

    int *buffer = (int*)calloc(buf_size, sizeof(int));
    // Pick some names from the list
    std::vector<unsigned long> name_indices;
    for (int i = 0; i < buf_size; i++) {
        unsigned long name_idx = ((i * i) + 2*i) % all_names.size();
        name_indices.push_back(name_idx);
    }

    // Push some random values to the buffer
    for (unsigned long idx : name_indices) {
        buffer[hash_djb2(all_names[idx]) % buf_size] = std::rand() % 10000;
    }

    // Compute the average, min and max
    float avg = .0f;
    int min = RAND_MAX;
    int max = 0.0f;
    for (unsigned long idx : name_indices) {
        int n = buffer[hash_djb2(all_names[idx]) % buf_size];
        avg += n;
        min = (min < n) ? min : n;
        max = (max > n) ? max : n;
    }
    avg /= buf_size;
    printf("Average: %.2f  Min: %i  Max: %i\n", avg, min, max);

    return 0;
}
