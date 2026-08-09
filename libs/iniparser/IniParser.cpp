#include "IniParser.h"

IniParser::IniParser(std::string str) : file_name(str) {
    parseFile(this->file_name);  
}
IniParser::~IniParser() {
    if (this->file.is_open()) {
        this->file.close();
    }
}

bool IniParser::hasSection(const std::string &section_name) const
{
    auto it = std::find_if(this->SectionsData.begin(), this->SectionsData.end(),
        [&section_name](const SectionData& s) { return s.name == section_name; });
    return it != SectionsData.end();
}

bool IniParser::hasKey(const std::string &request_string) const
{
    if (!(checkDelim(request_string, '.') && !checkDubles(request_string, '.')))
        throw std::invalid_argument("Incorect request syntax");

    std::stringstream ss(request_string);
    std::vector<std::string> out;
    std::string s;

    while (std::getline(ss, s, '.'))
        out.push_back(s);
    
    if (out.size() != 2) return false;

    return hasKey(out[0], out[1]); // reuse existing two-arg version
}

bool IniParser::hasKey(const std::string &section_name, const std::string &key) const
{
    auto it = std::find_if(this->SectionsData.begin(), this->SectionsData.end(),
        [&section_name](const SectionData& s) { return s.name == section_name; });
    if (it == SectionsData.end()) return false;
    return it->var_val.find(key) != it->var_val.end();
}

void IniParser::parseFile(std::string &f_name)
{
    this->file.open(f_name);
    if (!file) {throw std::invalid_argument(f_name + " could not be opened");}
    std::string string{};
    SectionData currentSection{};

    while (getline(this->file, string)) {
        ++line_count;
        if (string.empty()) { //пропускаем пустую строку сразу
            continue;
        }
        trimString(string);
        removeComment(string); 
            //удаляем пробелы по краям строки, удаляем комментарии
            //(внутренние пробелы больше не вырезаются здесь — это делается
            // отдельно для _var/_value ниже, чтобы можно было сохранить
            // пробелы внутри значений в кавычках)
        
        if (string.empty()) { //пустая строка или строка после удаления комментариев
            continue;
        } else if (string[0] == '[' && string[string.size() - 1] == ']' && !checkDelim(string, '=') && !checkDubles(string, '[',']')) { //проверяем если секция ограничена "[]", нет ли знака "=", есть ли дубли "[]"
            currentSection.name.clear();
            
            auto end = string.find_first_of(']');
            if (end != std::string::npos) {
                std::string read_name = string.substr(1, end - 1); //откидываем "[]" и записываем имя секции из текущей строки
                trimString(read_name); // на случай "[ section ]" с пробелами по краям
                currentSection.name = read_name;
                auto it = std::find_if(SectionsData.begin(), SectionsData.end(), [&read_name](SectionData &a) {return a.name == read_name;}); //проверяем вектор структур на наличие имени секции, если не найдено указывает на .end()
                if (it == SectionsData.end()) { //не найдено - добавляем в вектор структур
                    SectionsData.push_back(currentSection);
                }//если нашли то ничего не пишем, так как уже есть такая секция в массиве - else {it->name = read_name;} 
            }
            else {
                throw std::invalid_argument("\tLine " + std::to_string(line_count) + ". Error in syntax. Check '[]'\n");
            }
        } else if (!string.empty() && !checkDelim(string, '[', ']') && !checkDubles(string, '=')) { //начинается строка с данными переменных, проверяем нет ли "[]", есть ли дубли "="
            currentSection.var_val.clear();
            const auto end = string.find_first_of("=");
            if (end != std::string::npos) { 
                std::string _var = string.substr(0, end);
                std::string _value = string.substr(end + 1);

                trimString(_var);
                trimString(_value);

                if (_value.size() >= 2 && _value.front() == '"' && _value.back() == '"') {
                    // Значение в кавычках: убираем сами кавычки, но сохраняем
                    // все внутренние пробелы как есть (например "X, Y")
                    _value = _value.substr(1, _value.size() - 2);
                } else {
                    // Без кавычек: сохраняем прежнее поведение — вырезаем все
                    // пробелы, так как это нужно для списков через запятую
                    // (X, Y, Z -> X,Y,Z)
                    auto noSpaceEnd = std::remove(_value.begin(), _value.end(), ' ');
                    _value.erase(noSpaceEnd, _value.end());
                }

                std::string read_name = currentSection.name;
                auto it = std::find_if(SectionsData.begin(), SectionsData.end(), [&read_name](SectionData &a) {return a.name == read_name;});
                if (it->var_val.find(_var) == it->var_val.end()){
                    it->var_val.insert({_var, _value});
                } else {
                    it->var_val[_var]=_value;
                }
            } else { //если не нашли "="
                throw std::invalid_argument("\tLine " + std::to_string(line_count) + ". Error in syntax.  Check '='\n");
            }
        } else {
            throw std::invalid_argument("\tLine " + std::to_string(line_count) + ". Error in syntax.\n");
        }
    }
}

SectionData* IniParser::getSecPtr(const std::string& section_name) { //функция возвращения указателя на позицию имени_секции в массиве структур
    std::vector<SectionData>::iterator it = std::find_if (SectionsData.begin(), SectionsData.end(), [section_name](const SectionData& sect) {return sect.name.compare(section_name) == 0;} );
    return it != SectionsData.end() ? &*it : nullptr;
}

void IniParser::trimString(std::string &str) {
    // Обрезает только пробельные символы по краям строки (включая обычный
    // пробел), не трогая внутренние пробелы. Раньше эта функция вырезала
    // ВСЕ пробелы из строки целиком, что не позволяло хранить строковые
    // значения вида "Wall Thickness" с сохранением форматирования.
    std::string::size_type begin = str.find_first_not_of(" \t\n\v\f\r");
    if (begin == std::string::npos) { // строка состоит только из пробельных символов
        str.clear();
        return;
    }
    std::string::size_type end = str.find_last_not_of(" \t\n\v\f\r");
    str = str.substr(begin, end - begin + 1);
															   
									 
}

void IniParser::removeComment(std::string& str) {
    auto commentPosition = str.find(";");
    if (std::string::npos != commentPosition) {
        str = str.substr(0, commentPosition);
    }
}

bool IniParser::checkDelim (const std::string &_str, char _a) const { //проверка разделителя в строке
    auto check = _str.find_first_of(_a);
    if (check != std::string::npos) {
        return true;
    }
    return false;
}
bool IniParser::checkDelim (const std::string &_str, char _a, char _b) const { //проверка двух разделителей в строке
    auto check1 = _str.find_first_of(_a), check2 = _str.find_first_of(_b);
    if (check1 != std::string::npos || check2 != std::string::npos) {
        return true;
    }
    return false;
}

bool IniParser::checkDubles (const std::string &_str, char _a) const { //проверка дублей разделителя в строке
    auto check_left = _str.find_first_of(_a), check_right = _str.find_last_of(_a);
    if (check_left != check_right) {
        return true;
    }
    return false;
}

bool IniParser::checkDubles (const std::string &_str, char _a, char _b) const { //проверка дублей двух разделителей в строке
    auto check_left = _str.find_first_of(_a), check_right = _str.find_last_of(_a);
    if (check_left != check_right) {
        return true;
    }

    check_left = _str.find_first_of(_b), check_right = _str.find_last_of(_b);
    if (check_left != check_right) {
        return true;
    }

    return false;
}

const std::string IniParser::getKeyValue(const std::string& _sec_name, const std::string& _var_name) {
    SectionData* section = getSecPtr(_sec_name);
    if (section != nullptr) {
        const auto it = section->var_val.find(_var_name);
        if (it != section->var_val.end()) {
            if (it->second != "")
                return it->second;
            else
                throw std::invalid_argument("Variable " + _var_name + " in " + _sec_name + " has no value");
        }
        else {
            throw std::invalid_argument("Variable " + _var_name + " was not found in " + _sec_name);
        }
    }
    throw std::invalid_argument("Such section does not exist");
}

std::string IniParser::getKeyValue(const std::string &request) {
    if (!(checkDelim(request, '.') && !checkDubles(request, '.'))) {
        throw std::invalid_argument("Incorect request syntax"); 
    }

    std::stringstream ss(request);
    std::vector<std::string> out;
    std::string s;

    while (std::getline(ss, s, '.')) {
        out.push_back(s);
    }
    std::string value = getKeyValue(out[0],out[1]);
    return value;
}

std::vector<std::string> IniParser::splitValue(const std::string& raw, char delim) {
    // trimString() больше не вырезает пробелы внутри всей строки целиком —
    // вырезание внутренних пробелов теперь выполняется точечно для
    // НЕ-квотированных значений в parseFile(), так что raw здесь по-прежнему
    // приходит уже без пробелов для обычных списков, например "1,3,7" или "a,b,c".
    //
    // Для квотированных значений (например prj_cols_header = "Pprj, Pwork, Pallow")
    // внутренние пробелы сохраняются намеренно, поэтому после разбиения по
    // запятой токены вида " Pwork" нужно триммить по краям здесь — иначе
    // getValue<std::vector<int>> проходит незаметно (std::stoi сам пропускает
    // пробелы), а getValue<std::vector<std::string>> получит пробел внутри
    // строки, что молча ломает сравнения и вывод.
    std::vector<std::string> tokens;
    std::istringstream ss(raw);
    std::string token;
    while (std::getline(ss, token, delim)) {
        auto begin = token.find_first_not_of(' ');
        auto end = token.find_last_not_of(' ');
        if (begin != std::string::npos)
            token = token.substr(begin, end - begin + 1);
        else
            token.clear();

        if (!token.empty())
            tokens.push_back(token);
    }
    return tokens;
}