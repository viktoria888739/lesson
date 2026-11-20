#include <iostream>
#include <string>

enum class InputStatus { Value, EndOfFile };

enum class RoundStatus { Next, EofIncomplete, EofComplete };

struct Question {
    const char* stage;
    const char* title;
    const char* text;
    const char* code;
    const char* options[3];
    int correct;
    const char* explanation;
};

const char* kRawArrayCode =
    "struct RawArray {\n"
    "    int* data = new int[3]{10, 20, 30};\n"
    "    ~RawArray() { delete[] data; }\n"
    "};\n"
    "\n"
    "RawArray a;\n"
    "RawArray b = a;\n"
    "b.data[0] = 99;";

const char* kAssignCode =
    "IntArray& operator=(const IntArray& other) {\n"
    "    int* fresh = other.size ? new int[other.size] : nullptr;\n"
    "    for (std::size_t i = 0; i < other.size; ++i)\n"
    "        fresh[i] = other.data[i];\n"
    "    delete[] data;\n"
    "    data = fresh;\n"
    "    size = other.size;\n"
    "    return *this;\n"
    "}";

const char* kIndexCode =
    "int& operator[](std::size_t i) { return data[i]; }\n"
    "const int& operator[](std::size_t i) const { return data[i]; }";

const Question kQuestions[] = {
    {"Опасная копия", "Копирование указателя",
     "Выполняется RawArray b = a. Что делает неявный конструктор копирования с полем data?",
     kRawArrayCode,
     {"Создаёт новый буфер и копирует в него элементы.",
      "Копирует значение указателя: оба объекта ссылаются на один буфер.",
      "Не компилируется, потому что объявлен деструктор."},
     2,
     "Неявный конструктор копирования копирует адрес в data и не делает поэлементную копию буфера. В итоге два объекта делят владение буфером, хотя у этой памяти должен быть один владелец."},
    {"Опасная копия", "Общие данные",
     "Сразу после записи b.data[0] = 99, до разрушения объектов, какое значение хранится в a.data[0]?",
     kRawArrayCode,
     {"99, потому что оба указателя ведут к одному элементу.",
      "10, потому что b всегда имеет независимый буфер.",
      "Такая запись не компилируется."},
     1,
     "a.data и b.data хранят один адрес, так что запись в b.data[0] меняет общий элемент. До разрушения объектов a.data[0] равен 99."},
    {"Опасная копия", "Разрушение объектов",
     "Что неправильно при уничтожении a и b, если их указатели всё ещё содержат один адрес?",
     kRawArrayCode,
     {"Ничего: delete[] автоматически считает владельцев.",
      "Второй delete[] обязан бросить std::bad_alloc.",
      "Один буфер освобождается повторно, возникает undefined behavior."},
     3,
     "Оба деструктора вызывают delete[] для одного буфера, и повторное освобождение — неопределённое поведение. Стандарт не обещает при этом ни аварийное завершение, ни какой-либо конкретный вывод."},
    {"Правильное владение", "Порядок присваивания",
     "В этом операторе копируются int. Какое утверждение верно?", kAssignCode,
     {"До new надо выполнить delete[] data, иначе старый буфер утечёт.",
      "Если new бросит, объект сохранится; a = a здесь также безопасно.",
      "Любое a = a вызывает double free, если нет отдельного if."},
     2,
     "Копирующий конструктор создаёт новый объект, а копирующее присваивание заменяет значение уже существующего. Здесь сначала делается поэлементная копия int в новый буфер и только потом delete[]: если new бросит, владение буфером сохранится, а a = a безопасно."},
    {"Правильное владение", "Правила трёх, пяти и нуля",
     "Какой подход к специальным членам класса корректен?", nullptr,
     {"Согласовать деструктор, copy и при необходимости move; при владении через RAII-поля предпочесть правило нуля.",
      "Деструктор автоматически запрещает copy и реализует move.",
      "Правило нуля запрещает классу владеть std::vector."},
     1,
     "Деструктор, копирование и при необходимости перемещение нужно согласовать между собой. Если ресурсом владеют RAII-поля, например std::vector, лучше правило нуля: специальные члены сгенерирует компилятор."},
    {"Передача ресурсов", "Смысл std::move",
     "Что делает само выражение std::move(a)?", nullptr,
     {"Немедленно обнуляет все поля a.",
      "Безусловно вызывает перемещающий конструктор.",
      "Даёт xvalue; изменение ресурса выполняет выбранная затем операция."},
     3,
     "Само выражение std::move(a) лишь получает xvalue и не обнуляет поля. Ресурс меняет уже следующая выбранная операция; std::move не вызывает перемещающий конструктор безусловно."},
    {"Передача ресурсов", "Перемещающее присваивание",
     "Предположим, data - единолично владеющий int*, size - размер. Какой план подходит для operator=(IntArray&& other) с контрактом «self-move сохраняет значение»?",
     nullptr,
     {"Если this == &other, вернуть *this. Иначе освободить свой буфер, принять поля other, обнулить other и вернуть *this.",
      "Скопировать поля other, оставив источник без изменений, чтобы оба объекта владели буфером.",
      "Вызвать delete[] other.data, затем скопировать этот адрес в data."},
     1,
     "При контракте «self-move сохраняет значение» совпадение this и &other нужно обработать отдельно и вернуть *this. Иначе старый буфер освобождают, владение принимают от other, а источник обнуляют, чтобы буфер не оказался у двух объектов."},
    {"Передача ресурсов", "Состояние источника",
     "Контракт учебного IntArray: после перемещения в другой объект источник имеет size == 0 и data == nullptr. Какие действия допустимы?",
     nullptr,
     {"Объект уже уничтожен; использовать его нельзя никак.",
      "Прочитать size, проверить data, присвоить новое значение или уничтожить объект; a[0] читать нельзя.",
      "Прочитать a[0]: прежний элемент обязательно сохранился в источнике."},
     2,
     "Источник — живой объект после перемещения: по контракту учебного IntArray у него size == 0 и data == nullptr, поэтому можно прочитать size, проверить data, присвоить новое значение или уничтожить объект, но a[0] читать нельзя. Пустота не обещана произвольному типу C++; допустимость действий задают контракт типа и предусловие индексации."},
    {"Операторы под контролем", "Сравнение содержимого",
     "a и b независимо владеют буферами {10, 20, 30}. Их адреса различны. operator== сравнивает размер и элементы. Чему равно a == b?",
     nullptr,
     {"false, потому что адреса различаются.",
      "Выражение нельзя вычислить для двух объектов.",
      "true, потому что размеры и значения элементов совпадают."},
     3,
     "operator== сравнивает размер и значения элементов, а не адреса буферов. Содержимое совпадает, поэтому a == b равно true, хотя объекты владеют разными буферами."},
    {"Операторы под контролем", "Константная индексация",
     "Есть две перегрузки ниже. a - обычный массив, c - const-массив, оба размера 3. Какое утверждение верно?",
     kIndexCode,
     {"c[0] = 5 допустимо, потому что метод const.",
      "a[0] = 5 и int x = c[0] допустимы; c[0] = 5 не компилируется.",
      "a[10] = 5 обязательно бросит исключение."},
     2,
     "a[0] = 5 меняет элемент через неконстантный operator[], а int x = c[0] читает элемент через константную перегрузку; присваивание c[0] = 5 не компилируется, потому что она возвращает const int&. Выход за размер, например a[10], нарушает предусловие индексации и не обязан бросать исключение."},
};

constexpr int kQuestionCount = sizeof(kQuestions) / sizeof(kQuestions[0]);
static_assert(kQuestionCount == 10, "В игре ровно 10 вопросов.");

std::string trim(const std::string& text) {
    std::size_t begin = 0;
    while (begin < text.size() &&
           (text[begin] == ' ' || text[begin] == '\t' || text[begin] == '\r')) {
        ++begin;
    }
    std::size_t end = text.size();
    while (end > begin &&
           (text[end - 1] == ' ' || text[end - 1] == '\t' || text[end - 1] == '\r')) {
        --end;
    }
    return text.substr(begin, end - begin);
}

InputStatus readChoice(const char* prompt, int minValue, int maxValue, int& choice) {
    while (true) {
        std::cout << prompt << std::flush;
        std::string line;
        if (!std::getline(std::cin, line)) {
            return InputStatus::EndOfFile;
        }
        const std::string trimmed = trim(line);
        if (trimmed.size() == 1) {
            const char digit = trimmed[0];
            const char low = static_cast<char>('0' + minValue);
            const char high = static_cast<char>('0' + maxValue);
            if (digit >= low && digit <= high) {
                choice = digit - '0';
                return InputStatus::Value;
            }
        }
        std::cout << "Нужна одна цифра от " << minValue << " до " << maxValue
                  << ". Повторите ввод.\n";
    }
}

InputStatus waitForEnter() {
    while (true) {
        std::cout << "Нажмите Enter, чтобы продолжить.\n" << std::flush;
        std::string line;
        if (!std::getline(std::cin, line)) {
            return InputStatus::EndOfFile;
        }
        if (trim(line).empty()) {
            return InputStatus::Value;
        }
    }
}

const char* gradeText(int score) {
    if (score >= 90) {
        return "Отличное понимание темы";
    }
    if (score >= 70) {
        return "Уверенное понимание материала";
    }
    if (score >= 50) {
        return "Базовые знания, рекомендуется повторение";
    }
    return "Стоит повторить материал";
}

void showMenu() {
    std::cout << "\nМастерская памяти\n"
              << "1 — Начать игру\n"
              << "2 — Правила и баллы\n"
              << "0 — Выход\n";
}

void showRules() {
    std::cout << "\nПравила\n"
              << "Игра из 10 вопросов о владении памятью: копирование, перемещение и операторы.\n"
              << "На вопрос отвечайте одной цифрой 1, 2 или 3.\n"
              << "Пробелы и табуляции до и после цифры допустимы.\n"
              << "Пустая строка, лишние символы и числа вне диапазона не принимаются: "
              << "вопрос и счёт не меняются.\n"
              << "Верный ответ даёт 10 баллов, неверный — 0. Повторно ответить на вопрос нельзя.\n"
              << "Максимум 100 баллов. Результат между запусками программы не сохраняется.\n"
              << "После пояснения нажмите Enter. Строка только из пробелов тоже продолжает игру; "
              << "любой другой текст снова просит нажать Enter.\n"
              << "\nОценки\n"
              << "90-100 — Отличное понимание темы\n"
              << "70-89 — Уверенное понимание материала\n"
              << "50-69 — Базовые знания, рекомендуется повторение\n"
              << "0-49 — Стоит повторить материал\n";
}

void showResult(int correct) {
    const int score = correct * 10;
    std::cout << "\nРезультат\n"
              << "Правильно: " << correct << " из 10\n"
              << "Баллы: " << score << " из 100\n"
              << gradeText(score) << "\n";
}

RoundStatus askQuestion(const Question& question, int index, int& correct) {
    std::cout << "\nЭтап: " << question.stage << "\n"
              << "Вопрос " << (index + 1) << "/10. " << question.title << "\n"
              << question.text << "\n";
    if (question.code != nullptr) {
        std::cout << "\n" << question.code << "\n";
    }
    std::cout << "\n";
    for (int option = 0; option < 3; ++option) {
        std::cout << (option + 1) << ") " << question.options[option] << "\n";
    }

    int answer = 0;
    if (readChoice("Ваш ответ: ", 1, 3, answer) == InputStatus::EndOfFile) {
        return RoundStatus::EofIncomplete;
    }

    if (answer == question.correct) {
        ++correct;
        std::cout << "Верно\n";
    } else {
        std::cout << "Неверно\n";
    }
    std::cout << "Правильный ответ: " << question.correct << ") "
              << question.options[question.correct - 1] << "\n"
              << "Пояснение: " << question.explanation << "\n";

    if (waitForEnter() == InputStatus::EndOfFile) {
        if (index + 1 == kQuestionCount) {
            return RoundStatus::EofComplete;
        }
        return RoundStatus::EofIncomplete;
    }
    return RoundStatus::Next;
}

bool playGame() {
    int correct = 0;
    for (int index = 0; index < kQuestionCount; ++index) {
        const RoundStatus status = askQuestion(kQuestions[index], index, correct);
        if (status == RoundStatus::EofIncomplete) {
            std::cout << "\nИгра прервана.\n"
                      << "Итоговая оценка за неполную игру не выставляется.\n";
            return false;
        }
        if (status == RoundStatus::EofComplete) {
            showResult(correct);
            return false;
        }
    }
    showResult(correct);
    return true;
}

int main() {
    while (true) {
        showMenu();
        int choice = 0;
        if (readChoice("Ваш выбор: ", 0, 2, choice) == InputStatus::EndOfFile) {
            std::cout << "\nВвод завершён.\n";
            return 0;
        }
        if (choice == 0) {
            std::cout << "Выход.\n";
            return 0;
        }
        if (choice == 2) {
            showRules();
            continue;
        }
        if (!playGame()) {
            return 0;
        }
    }
}
