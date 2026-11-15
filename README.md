# Práctica de los Temas 1 y 2

## Patrones elegidos y por qué

### Patrones de creación

**Singleton** para el Dice (dado). Ya que el juego tira dados constantemente, al tener una única fuente de aleatoriedad, podemos evitar crear un generador en cada parte en donde sea necesario este azar.

**Fábrica** para el EnemyFactory. Añadir un enemigo nuevo es añadir una fábrica nueva, ya que el código no tiene porque conocer clases concretas, solo generar enemigos que llenen salas.

El **Builder** para el LevelBuilder, DungeonBuilder y LeverDirector. El proceso es exactamente el mismo pero con este patrón podemos implementar el azar.

### Patrones de comportamiento

**Comando** para el ICommand (MoveCommand, AttackCommand, PickupCommand, WeaponCommand, QuitCommand). El jugador puede atacar, moverse, recoger oro y cambiar de arma. Cada acción es un objeto y las teclas se asocian a comandor en un solo sitio. Si necesitas reasignar teclas o añadir acciones no tienes que tocar la lógica del juego. Esto también nos permite hacer un Undo o poder grabar partidas. El Invoker dentro de esto es el InputHandler.

**Estrategia** para el IDamageStrategy, lo que nos lleva al PhysicalDamage y al ElementalDamage. Esto se debe a que se cambia a tiempo de ejecución en C debido a que el arma equipada hace que varie el dado que se tira.

**Estado** para IMood y los estados (CalmMood, AngryMood, FuriousMood (para los Orcos (en Orc)) es para poder diferenciar cuántos ataques tiene que hacer el orco dependiendo de su humor. Esto también nos sirve para poder evitar una cadena de ifs eterna para preguntar que tiene que hacer el orco. 

**Observador** para Publisher/IObserver. Sus suscriptores son MessageLog y Game. De esta forma, todos los procesos que tienen que ocurrir cuando un enemigo muere no se satura el programa y el combate solo publica el evento de EnemyKilled. Esto hace que no conozca quien reaccione y así reducir la saturación. 
