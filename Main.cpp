#define NOMINMAX
#include <algorithm>
#include <cstdlib>
#include <deque>
#include <iostream>
#include <map>
#include <memory>
#include <random>
#include <string>
#include <vector>
#ifdef _WIN32
#include <Windows.h>
#endif
using namespace std;
//singleton para el dice
class Dice {
public:
    static Dice& get() { static Dice instance; return instance; }  // se crea al primer uso
    Dice(const Dice&) = delete;
    Dice& operator=(const Dice&) = delete;
    int roll(int sides) { return range(1, sides); }                
    int range(int a, int b) { return uniform_int_distribution<int>(a, b)(rng); }
private:
    Dice() : rng(random_device{}()) {}
    mt19937 rng;
};

// observer para publicar los eventos del juego (muertes, logs, etc)
enum class EventType { Log, EnemyKilled };
struct GameEvent {
    EventType type;
    string text;
    int level = 0, treasurePct = 0, x = 0, y = 0;
    bool boss = false;
};
class IObserver {
public:
    virtual ~IObserver() = default;
    virtual void onEvent(const GameEvent& e) = 0;
};
class Publisher {
    vector<IObserver*> subs;
public:
    void subscribe(IObserver* o) { subs.push_back(o); }
    void unsubscribe(IObserver* o) { subs.erase(remove(subs.begin(), subs.end(), o), subs.end()); }
    void notify(const GameEvent& e) { for (auto* s : subs) s->onEvent(e); }
    void log(const string& t) { notify({ EventType::Log, t }); }
};
class MessageLog : public IObserver {          // suscriptor 1: guarda los ultimos mensajes
    deque<string> lines;
public:
    void onEvent(const GameEvent& e) override {
        if (e.type != EventType::Log) return;
        lines.push_back(e.text);
        if (lines.size() > 6) lines.pop_front();
    }
    const deque<string>& get() const { return lines; }
};

//estrategia para calcular el daño
class IDamageStrategy {
public:
    virtual ~IDamageStrategy() = default;
    virtual int roll(string& detail) const = 0;
    virtual string name() const = 0;
};
class PhysicalDamage : public IDamageStrategy {
    int sides;
public:
    explicit PhysicalDamage(int s) : sides(s) {}
    int roll(string& d) const override {
        int r = Dice::get().roll(sides);
        d = "d" + to_string(sides) + "=" + to_string(r);
        return r;
    }
    string name() const override { return "d" + to_string(sides); }
};
class ElementalDamage : public IDamageStrategy {
    int sides;
    vector<string> elements;
public:
    ElementalDamage(int s, vector<string> e) : sides(s), elements(move(e)) {}
    int roll(string& d) const override {
        int r = Dice::get().roll(sides), total = r;
        d = "d" + to_string(sides) + "=" + to_string(r);
        for (auto& el : elements) {                      // un d4 extra por elemento
            int x = Dice::get().roll(4);
            total += x;
            d += " +" + el + " d4=" + to_string(x);
        }
        return total;
    }
    string name() const override {
        string n = "d" + to_string(sides);
        for (auto& e : elements) n += "+d4 " + e;
        return n;
    }
};
struct Weapon { string name; shared_ptr<IDamageStrategy> dmg; };

// patron state para el comportamiento de los orcos
class IMood {
public:
    virtual ~IMood() = default;
    virtual int attacks() const = 0;
    virtual string name() const = 0;
    virtual shared_ptr<IMood> evolve(double hpRatio) const = 0;  // nullptr = no cambia
};
class FuriousMood : public IMood {
public:
    int attacks() const override { return 3; }
    string name() const override { return "furioso"; }
    shared_ptr<IMood> evolve(double) const override { return nullptr; }
};
class AngryMood : public IMood {
public:
    int attacks() const override { return 2; }
    string name() const override { return "enfadado"; }
    shared_ptr<IMood> evolve(double r) const override {
        if (r < 0.33) return make_shared<FuriousMood>();
        return nullptr;
    }
};
class CalmMood : public IMood {
public:
    int attacks() const override { return 1; }
    string name() const override { return "tranquilo"; }
    shared_ptr<IMood> evolve(double r) const override {
        if (r < 0.33) return make_shared<FuriousMood>();
        if (r < 0.66) return make_shared<AngryMood>();
        return nullptr;
    }
};

class Enemy {
public:
    string name;
    char glyph;
    int level = 1, treasurePct, baseAc, baseHp, hpPerLevel, ac = 0, hp = 0, maxHp = 0, x = 0, y = 0;
    shared_ptr<IDamageStrategy> attack;

    Enemy(string n, char g, int treasure, int bAc, int bHp, int hpl, shared_ptr<IDamageStrategy> a)
        : name(move(n)), glyph(g), treasurePct(treasure), baseAc(bAc), baseHp(bHp), hpPerLevel(hpl), attack(move(a)) {
        rescale(1);
    }
    virtual ~Enemy() = default;
    void rescale(int l) { level = l; ac = baseAc + l; maxHp = hp = baseHp + hpPerLevel * l; }
    // para el patron prototipo
	virtual unique_ptr<Enemy> clone() const = 0;             
    virtual int attacksPerTurn() const { return 1; }
    virtual bool isBoss() const { return false; }
    virtual string label() const { return name; }
    virtual void onDamaged(Publisher&) {}
};
class Slime : public Enemy {
    vector<string> elements;
public:
    Slime() : Enemy("Slime", 'S', 30, 8, 8, 4, make_shared<PhysicalDamage>(4)) {}
    unique_ptr<Enemy> clone() const override { return make_unique<Slime>(*this); }
    void addElement(const string& el) {                       // variante elemental
        elements.push_back(el);
        attack = make_shared<ElementalDamage>(4, elements);
        name = "Slime de " + el;
    }
};
class Orc : public Enemy {
    shared_ptr<IMood> mood;                                   // state actual
public:
    explicit Orc(shared_ptr<IMood> m)
        : Enemy("Orco", 'O', 50, 9, 15, 6, make_shared<PhysicalDamage>(8)), mood(move(m)) {
    }
    unique_ptr<Enemy> clone() const override { return make_unique<Orc>(*this); }
    int attacksPerTurn() const override { return mood->attacks(); }
    string label() const override { return name + " " + mood->name(); }
    void onDamaged(Publisher& bus) override {                 // al recibir dano puede cambiar de estado
        auto next = mood->evolve(double(hp) / maxHp);
        if (next) { mood = next; bus.log(name + " se pone " + mood->name() + "!"); }
    }
};
class Dragon : public Enemy {
public:
    Dragon() : Enemy("Dragon", 'D', 100, 14, 40, 15, make_shared<ElementalDamage>(10, vector<string>{"fuego"})) {}
    unique_ptr<Enemy> clone() const override { return make_unique<Dragon>(*this); }
    bool isBoss() const override { return true; }
};

//patron fabrica para crear enemigos 
class EnemyFactory {
public:
    virtual ~EnemyFactory() = default;
    unique_ptr<Enemy> create(int level) { auto e = make(); e->rescale(level); return e; }
protected:
    virtual unique_ptr<Enemy> make() = 0;
};
class SlimeFactory : public EnemyFactory {
    vector<unique_ptr<Slime>> protos;
public:
    SlimeFactory() {
        protos.push_back(make_unique<Slime>());
        for (const char* el : { "fuego", "agua", "hielo", "electro" }) {
            auto s = make_unique<Slime>();
            s->addElement(el);
            protos.push_back(move(s));
        }
    }
protected:
    unique_ptr<Enemy> make() override { return protos[Dice::get().range(0, int(protos.size()) - 1)]->clone(); }
};
class OrcFactory : public EnemyFactory {
    vector<unique_ptr<Orc>> protos;
public:
    OrcFactory() {
        protos.push_back(make_unique<Orc>(make_shared<CalmMood>()));
        protos.push_back(make_unique<Orc>(make_shared<AngryMood>()));
        protos.push_back(make_unique<Orc>(make_shared<FuriousMood>()));
    }
protected:
    unique_ptr<Enemy> make() override { return protos[Dice::get().range(0, int(protos.size()) - 1)]->clone(); }
};
class DragonFactory : public EnemyFactory {
    Dragon proto;
protected:
    unique_ptr<Enemy> make() override { return proto.clone(); }
};

//mapa 
struct Pos { int x, y; };
struct Door { int x, y, target; };
struct Gold { int x, y, amount; };
struct Room {
    int w, h;
    vector<Door> doors;
    vector<Gold> gold;
    vector<unique_ptr<Enemy>> enemies;
    Room(int w_, int h_) : w(w_), h(h_) {}
    Enemy* enemyAt(int x, int y) const {
        for (auto& e : enemies) if (e->x == x && e->y == y) return e.get();
        return nullptr;
    }
    bool occupied(int x, int y) const {
        for (auto& d : doors) if (d.x == x && d.y == y) return true;
        for (auto& g : gold) if (g.x == x && g.y == y) return true;
        return enemyAt(x, y) != nullptr;
    }
    Pos freePos() const {
        int x, y;
        do { x = Dice::get().range(0, w - 1); y = Dice::get().range(0, h - 1); } while (occupied(x, y));
        return { x, y };
    }
};
struct Level { vector<Room> rooms; int number = 1; };
enum class Difficulty { Easy, Normal, Hard };

//builder para crear niveles de mazmorra
class LevelBuilder {
public:
    virtual ~LevelBuilder() = default;
    virtual void reset(int levelNumber) = 0;
    virtual void buildRooms(Difficulty d) = 0;
    virtual void connectRooms() = 0;
    virtual void placeEnemies() = 0;
    virtual void placeBoss() = 0;
    virtual void placeGold() = 0;
    virtual unique_ptr<Level> getResult() = 0;
};
class DungeonBuilder : public LevelBuilder {
    unique_ptr<Level> lvl;
    SlimeFactory slimes;                                      
    OrcFactory orcs;
    DragonFactory dragons;

    bool linked(int a, int b) {
        for (auto& d : lvl->rooms[a].doors) if (d.target == b) return true;
        return false;
    }
    void connect(int a, int b) {                              
        Room& A = lvl->rooms[a]; Room& B = lvl->rooms[b];
        Pos pa = A.freePos(); A.doors.push_back({ pa.x, pa.y, b });
        Pos pb = B.freePos(); B.doors.push_back({ pb.x, pb.y, a });
    }
    void put(Room& r, unique_ptr<Enemy> e) {
        Pos p = r.freePos(); e->x = p.x; e->y = p.y; r.enemies.push_back(move(e));
    }
public:
    void reset(int n) override { lvl = make_unique<Level>(); lvl->number = n; }
    void buildRooms(Difficulty d) override {
        int lo = 3, hi = 5;                                   
        if (d == Difficulty::Normal) { lo = 4; hi = 6; }
        if (d == Difficulty::Hard) { lo = 6; hi = 8; }
        int n = Dice::get().range(lo, hi);
        for (int i = 0; i < n; ++i)
            lvl->rooms.emplace_back(Dice::get().range(10, 20), Dice::get().range(5, 8));
    }
    void connectRooms() override {
        int n = int(lvl->rooms.size());
        for (int i = 1; i < n; ++i) connect(i, Dice::get().range(0, i - 1));   
        for (int k = 0; k < n / 2; ++k) {                                      
            int a = Dice::get().range(0, n - 1), b = Dice::get().range(0, n - 1);
            if (a != b && !linked(a, b)) connect(a, b);
        }
    }
    void placeEnemies() override {
        for (auto& r : lvl->rooms) {
            int count = Dice::get().range(0, 3);
            for (int i = 0; i < count; ++i) {
                EnemyFactory& f = (Dice::get().roll(100) <= 70) ? static_cast<EnemyFactory&>(slimes)
                    : static_cast<EnemyFactory&>(orcs);
                put(r, f.create(lvl->number));
            }
        }
    }
    void placeBoss() override {                               
        int idx = Dice::get().range(1, int(lvl->rooms.size()) - 1);
        put(lvl->rooms[idx], dragons.create(lvl->number));
    }
    void placeGold() override {
        for (auto& r : lvl->rooms) {
            int count = Dice::get().range(0, 2);
            for (int i = 0; i < count; ++i) {
                Pos p = r.freePos();
                r.gold.push_back({ p.x, p.y, Dice::get().range(5, 20) });
            }
        }
    }
    unique_ptr<Level> getResult() override { return move(lvl); }
};
class LevelDirector {
public:
    static unique_ptr<Level> build(LevelBuilder& b, int number, Difficulty d) {
        b.reset(number);
        b.buildRooms(d);
        b.connectRooms();
        b.placeEnemies();
        b.placeBoss();
        b.placeGold();
        return b.getResult();
    }
};


class Player {
public:
    int x = 0, y = 0, hp = 100, maxHp = 100, level = 1, xp = 0, gold = 0, hitBonus = 0, ac = 10;
    vector<Weapon> weapons;
    size_t cur = 0;
    Weapon& weapon() { return weapons[cur]; }
    void gainXp(int n, Publisher& bus) {
        xp += n;
        while (xp >= 10 * level) {                            
            xp -= 10 * level;
            ++level; maxHp += 10; hp = min(maxHp, hp + 10); ++hitBonus;
            bus.log("*** Subes al nivel " + to_string(level) + " (vida max " + to_string(maxHp) + ", +1 al golpe) ***");
        }
    }
};


static void clearScreen() {
#ifdef _WIN32
    system("cls");
#else
    system("clear");
#endif
}

class Game : public IObserver {
public:
    Player player;
    DungeonBuilder builder;
    unique_ptr<Level> level;
    int roomIdx = 0, levelNum = 1;
    Difficulty diff;
    Publisher bus;
    MessageLog log;
    bool running = true, cleared = false;

    explicit Game(Difficulty d) : diff(d) {
        bus.subscribe(&log);
        bus.subscribe(this);
        player.weapons = {
            { "Daga", make_shared<PhysicalDamage>(4) },
            { "Espada", make_shared<PhysicalDamage>(8) },
            { "Espada de fuego", make_shared<ElementalDamage>(6, vector<string>{"fuego"}) },
            { "Hacha de hielo y rayo", make_shared<ElementalDamage>(8, vector<string>{"hielo", "rayo"}) } };
        startLevel();
        bus.log("Bienvenido a la mazmorra. Mata al dragon para bajar.");
    }
    Room& room() { return level->rooms[roomIdx]; }

    void startLevel() {
        level = LevelDirector::build(builder, levelNum, diff);
        roomIdx = 0; cleared = false;
        Pos p = room().freePos(); player.x = p.x; player.y = p.y;
        bus.log("--- Nivel " + to_string(levelNum) + " (" + to_string(level->rooms.size()) + " salas) ---");
    }

    
    void onEvent(const GameEvent& e) override {
        if (e.type != EventType::EnemyKilled) return;
        player.gainXp(5 * e.level, bus);
        if (Dice::get().roll(100) <= e.treasurePct) {         
            room().gold.push_back({ e.x, e.y, Dice::get().range(5, 15) * e.level });
            bus.log("Algo brilla donde cayo el enemigo...");
        }
        if (e.boss) cleared = true;
    }

    
    bool tryMove(int dx, int dy) {
        Room& r = room();
        int nx = player.x + dx, ny = player.y + dy;
        if (nx < 0 || ny < 0 || nx >= r.w || ny >= r.h) return false;
        if (r.enemyAt(nx, ny)) { bus.log("Hay un enemigo en medio. Ataca con ESPACIO."); return false; }
        player.x = nx; player.y = ny;
        for (auto& d : r.doors) {
            if (d.x == nx && d.y == ny) {                     
                int from = roomIdx;
                roomIdx = d.target;
                for (auto& back : room().doors)
                    if (back.target == from) { player.x = back.x; player.y = back.y; break; }
                bus.log("Atraviesas la puerta hacia la sala " + to_string(roomIdx + 1) + ".");
                break;
            }
        }
        return true;
    }
    bool attack() {
        Room& r = room();
        int idx = -1;
        for (int i = 0; i < int(r.enemies.size()); ++i)
            if (max(abs(r.enemies[i]->x - player.x), abs(r.enemies[i]->y - player.y)) <= 1) { idx = i; break; }
        if (idx < 0) { bus.log("No hay enemigos al alcance."); return false; }
        Enemy* t = r.enemies[idx].get();
        int roll = Dice::get().roll(20) + player.hitBonus;    
        if (roll <= t->ac) {
            bus.log("Fallas a " + t->label() + " (" + to_string(roll) + " vs AC " + to_string(t->ac) + ").");
            return true;
        }
        string det;
        int dmg = player.weapon().dmg->roll(det);             
        t->hp -= dmg;
        bus.log("Golpeas a " + t->label() + " (" + to_string(roll) + " vs AC " + to_string(t->ac) + "): " +
            to_string(dmg) + " de dano [" + det + "]");
        if (t->hp <= 0) {
            GameEvent ev{ EventType::EnemyKilled, t->name + " muere", t->level, t->treasurePct, t->x, t->y, t->isBoss() };
            bus.log(t->name + " muere.");
            r.enemies.erase(r.enemies.begin() + idx);
            bus.notify(ev);                                   
        }
        else {
            t->onDamaged(bus);                                
        }
        return true;
    }
    bool pickup() {
        Room& r = room();
        for (size_t i = 0; i < r.gold.size(); ++i)
            if (r.gold[i].x == player.x && r.gold[i].y == player.y) {
                player.gold += r.gold[i].amount;
                bus.log("Recoges " + to_string(r.gold[i].amount) + " de oro.");
                r.gold.erase(r.gold.begin() + i);
                return true;
            }
        bus.log("Aqui no hay oro.");
        return false;
    }
    bool changeWeapon() {
        player.cur = (player.cur + 1) % player.weapons.size();
        bus.log("Equipas: " + player.weapon().name + " (" + player.weapon().dmg->name() + ")");
        return true;
    }

   
    void enemiesTurn() {
        for (auto& e : room().enemies) {
            int dx = player.x - e->x, dy = player.y - e->y;
            int dist = max(abs(dx), abs(dy));
            if (dist <= 1) {
                for (int k = 0; k < e->attacksPerTurn() && player.hp > 0; ++k) {   
                    int roll = Dice::get().roll(20);
                    if (roll <= player.ac) { bus.log(e->label() + " falla."); continue; }
                    string det;
                    int dmg = e->attack->roll(det);
                    player.hp -= dmg;
                    bus.log(e->label() + " te golpea: " + to_string(dmg) + " [" + det + "]");
                }
            }
            else if (dist <= 6) {                                                
                int sx = (dx > 0) - (dx < 0), sy = (dy > 0) - (dy < 0);
                int tries[3][2] = { { sx, sy },{ sx, 0 },{ 0, sy } };
                for (auto& t : tries) {
                    if (t[0] == 0 && t[1] == 0) continue;
                    int nx = e->x + t[0], ny = e->y + t[1];
                    if (nx < 0 || ny < 0 || nx >= room().w || ny >= room().h) continue;
                    if (nx == player.x && ny == player.y) continue;
                    if (room().enemyAt(nx, ny)) continue;
                    e->x = nx; e->y = ny; break;
                }
            }
        }
    }
    void endTurn() {
        enemiesTurn();
        if (player.hp <= 0) { player.hp = 0; running = false; bus.log("Has muerto. Fin de la partida."); return; }
        if (cleared) { ++levelNum; startLevel(); }
    }

    void draw() {
        clearScreen();
        Room& r = room();
        vector<string> g(r.h, string(r.w, '.'));
        for (auto& o : r.gold) g[o.y][o.x] = '$';
        for (auto& d : r.doors) g[d.y][d.x] = char('1' + d.target);  
        for (auto& e : r.enemies) g[e->y][e->x] = e->glyph;
        g[player.y][player.x] = '@';
        cout << '+' << string(r.w, '-') << "+\n";
        for (auto& row : g) cout << '|' << row << "|\n";
        cout << '+' << string(r.w, '-') << "+\n";
        cout << "Nivel:" << levelNum << "  Sala:" << roomIdx + 1 << '/' << level->rooms.size()
            << "  Hits:" << player.hp << '(' << player.maxHp << ")  Lvl:" << player.level
            << "  Gold:" << player.gold << "  Armor:" << player.ac
            << "  Arma:" << player.weapon().name << '\n';
        cout << "@ tu  S slime  O orco  D dragon  $ oro  1-8 puerta a esa sala\n";
        cout << "[Flechas] mover  [Espacio] atacar  [E] oro  [C] arma  [Esc] salir\n\n";
        for (auto& l : log.get()) cout << l << '\n';
        cout.flush();
    }
};

// patron comando para encapsular las acciones del jugador
class ICommand {
public:
    virtual ~ICommand() = default;
    virtual bool execute(Game& g) = 0;                       
};
class MoveCommand : public ICommand {
    int dx, dy;
public:
    MoveCommand(int x, int y) : dx(x), dy(y) {}
    bool execute(Game& g) override { return g.tryMove(dx, dy); }
};
class AttackCommand : public ICommand { public: bool execute(Game& g) override { return g.attack(); } };
class PickupCommand : public ICommand { public: bool execute(Game& g) override { return g.pickup(); } };
class WeaponCommand : public ICommand { public: bool execute(Game& g) override { return g.changeWeapon(); } };
class QuitCommand : public ICommand { public: bool execute(Game& g) override { g.running = false; return false; } };

enum class Key { Left, Right, Up, Down, Attack, Pickup, Weapon, Quit };


static Key readKey() {
#ifdef _WIN32
    struct KM { int vk; Key key; };
    static const KM map[] = { { VK_LEFT, Key::Left },{ VK_RIGHT, Key::Right },{ VK_UP, Key::Up },{ VK_DOWN, Key::Down },
                             { VK_SPACE, Key::Attack },{ 'E', Key::Pickup },{ 'C', Key::Weapon },{ VK_ESCAPE, Key::Quit } };
    static const int N = sizeof(map) / sizeof(map[0]);
    static bool prev[16] = {};
    for (;;) {
        for (int i = 0; i < N; ++i) {
            bool down = (GetKeyState(map[i].vk) & 0x8000) != 0;
            if (down && !prev[i]) { prev[i] = true; return map[i].key; }
            if (!down) prev[i] = false;
        }
        Sleep(10);
    }
#else
    char c;
    while (cin.get(c)) {
        switch (tolower(c)) {
        case 'a': return Key::Left;   case 'd': return Key::Right;
        case 'w': return Key::Up;     case 's': return Key::Down;
        case ' ': return Key::Attack; case 'e': return Key::Pickup;
        case 'c': return Key::Weapon; case 'q': return Key::Quit;
        }
    }
    return Key::Quit;
#endif
}

class InputHandler {                                          
    map<Key, unique_ptr<ICommand>> bindings;
public:
    InputHandler() {
        bindings[Key::Left] = make_unique<MoveCommand>(-1, 0);
        bindings[Key::Right] = make_unique<MoveCommand>(1, 0);
        bindings[Key::Up] = make_unique<MoveCommand>(0, -1);
        bindings[Key::Down] = make_unique<MoveCommand>(0, 1);
        bindings[Key::Attack] = make_unique<AttackCommand>();
        bindings[Key::Pickup] = make_unique<PickupCommand>();
        bindings[Key::Weapon] = make_unique<WeaponCommand>();
        bindings[Key::Quit] = make_unique<QuitCommand>();
    }
    ICommand* commandFor(Key k) {
        auto it = bindings.find(k);
        return it == bindings.end() ? nullptr : it->second.get();
    }
};


int main() {
    int choice = 2;
    cout << "Dificultad (1 facil, 2 normal, 3 dificil): ";
    cin >> choice;
    Difficulty d = choice == 1 ? Difficulty::Easy : choice == 3 ? Difficulty::Hard : Difficulty::Normal;

    Game game(d);
    InputHandler input;
    game.draw();
    while (game.running) {
        ICommand* cmd = input.commandFor(readKey());
        if (!cmd) continue;
        if (cmd->execute(game)) game.endTurn();              
        game.draw();
    }
    cout << "\nGracias por jugar.\n";
    return 0;
}