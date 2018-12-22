**CMPUT 350 – Project Report**  
Bot Name: Ammolite | Team Members: Shouyang Zhou, Seth Bergen  

**Motivation**

This project demonstrates three components, a project built in C++, tangible interaction with a third-party library/API, and interaction with AI design principles.

We felt that a Terran macro focused bot would lead to the best functional project, a deliverable bot, that suited all learning outcomes.

**Bot Overview**

Our bot implements a single early game focused macro strategy. The bot plays a standard bio-tank focused strategy that relies on Terran ground forces consisting in marines, marauders, and siege tanks. This unit composition synergizes well to handle most other unit compositions.

We aim for a bot that is reasonably challenging to play, and that mimics a human player&#39;s in-game decisions.

**Design Overview**

We structure our implementation conceptually based on a hierarchy of bot functions. They are as follows:

- Bot
  - Game Economy
    - Resource Management, Unit Building, Base Building, Upgrade Building
  - Game Fighting
    - Unit Management, Attacking, Defending
  - Knowledge Representation
    - Scouting

We consider the above to be the basis for a functionally complete bot.  As each faction within StarCraft II feature their own factional mechanics, we look at implementing these faction specific considerations as an additional bot feature. They are as follows:

- Resource Management – Bot implements the Terran M.U.L.E call down ability.
- Base Building – Bot implements Terran &quot;Add-on&quot; structures
- Unit Management – Bot uses Siege Mode Abilities for Siege Tanks and Wikings

**Acknowledgements**

Our bot inherits from the multiplayer bot class in the bot examples file and uses some of the functions innate to that class for convenience. We found the API&#39;s documentation to be poor but the bot examples to be a very good source of information.

**Experimental Results (of Bot Performance)**
We compare our bot to the innate bots provided via the StarCraft II API. We benchmark our bot&#39;s performance to those of the innate &quot;hard&quot; difficulty bot. We believe the performance characteristics to lie somewhere between the &quot;hard&quot; and &quot;very hard&quot; difficulty in comparison.

Hard

|   | Win | Draw | Loss |
| --- | --- | --- | --- |
| Zerg | 22 | 8 | 0 |
| Protoss | 23 | 7 | 0 |
| Terran | 29 | 1 | 0 |

Very Hard

|   | Win | Draw | Loss |
| --- | --- | --- | --- |
| Zerg | 0 | 11 | 19 |
| Protoss | 0 | 0 | 30 |
| Terran | 18 | 0 | 12 |

\* Draw awarded after 45 minutes elapsed game time without a winner.

**Implementation Overview**

Our bot is implemented within the bot.cc file, the bot itself is a subclass of the multiplayer bot class in the bot examples. Secondary helper functions are in the utils.cpp file.

Our bot implements a series of call back functions in the public OnStep() function used by the API. This is how we provide most of our functionality. To space out function calls, we keep a step counter and mod this step counter by prime numbers to space out infrequently needed functions.

**Game Economy Implementation**

Game economy actions are implemented via the BuildOrder() function, the ManageWorkers() function, and onUnit() event listeners.

We take a choreography approach to game economy functions to address the limitations a build order-based approach. Originally, we had wanted to implement a build order-based approach, however we discovered that the API does not return enough information to do this simply.

We implement our build order as a series of heuristics that the bot checks when considering building.  The bot takes into consideration its current supply situation, resource counts, base counts, and existing production facilities so to build new buildings. In the choreography context, this allows the bot to adapt to destroyed buildings since it will naturally try to replace those first given its heuristics ordering.

Production structures are managed via a callback function in the OnUnitIdle() event handler that the API calls upon. Like the build order, production structures use a series of heuristics to decide what to build. The bot will consider our current unit composition and the availability of resources to build new units.

Unit upgrades are managed via a separate function. Upon reaching a certain unit count and resource availability our bot will build unit upgrades relevant to its current army as a real player. We noticed that if we implement this as a callback function, the API does not reliably build upgrades.

Worker units are managed via the ManageWorkers() function and several helper functions provided in the bot examples. We implement functionality for the bot to determine and build to its ideal worker count and to use the M.U.L.E call down ability where possible. The bot examples provide helpful routines for balancing workers between bases, finding mineral patches for worker units and to try building structures at random points near bases.

**Game Fighting Implementation**

Unit management actions are dispatched in the OnStep() function and some unit event listeners.

The bot will calculate a rally point for its army units based off the center of the map and the bases it possesses. It will try to find a point facing the center of the map defending the farthest expansion from its starting point.  The bot will rally all army units not attacking or defending or scouting to this point.

The bot will scout periodically using marines by sending them to either a possible starting location for the enemy or a random position.  More on this in the knowledge representation section.

The bot will attack using the information gleamed from scouting. Using information about known enemy locations, the bot will periodically send a wave of attacking units towards a known enemy location from its pool of idle units. This process prioritizes enemy structures.

The bot will be defending whenever an enemy unit comes close to a base using units of its pool of idle units.

During combat, our bot uses the morph abilities of both its Siege Tanks and Vikings. The siege tanks will enter siege mode if there are a large number of enemy units within range and then un-siege when the coast is clear. The Vikings will remain in their flying, anti-air state while not in combat as well as in the case that the enemy units currently engaged include flying units. Otherwise, they will morph to their landed, anti-ground form.

**Knowledge Representation Implementation**

We found knowledge presentation to be the most conceptually difficult part of the bot. For the scope of this project we decided to handle this aspect simply.  Our design issues considered in this aspect the differences between how a person manages information versus the bot. To be concise in this topic let us consider how the following questions:

- How do I know the enemy&#39;s unit composition?
- How do I know where my enemy&#39;s forces are?
- How do I know if a fight is going well or was a good trade?
- How do I know if the enemy is doing something odd?

Intuitively a person can solve these questions easily via their previous experiences. We tried distilling some general knowledge into a couple of heuristic functions, but we found little progress here. Worse yet, if a heuristic is wrong.

In the end we decided upon a simple approach. The bot maintains a list of enemy unit positions that is updated via the onUnitEnteredVision() event handler. Enemy units entering vision are added to our list of known enemy positions if they are sufficiently far away from the base. Every minute or so this list is flushed of 90% of its contents. We found this naïve approach to be surprisingly durable. We flush the list to account for information decay but not fully such that the bot still has a few targets to choose from.

Our attack function chooses from this list of enemy unit positions probabilistically. We decided upon this approach due to some quirks the bot had experienced early on. When scouting units intermingle, our bot would sometimes choose to attack some point utterly irrelevant game-wise e since it was a location of an enemy scout far away from anything of importance. By choosing probabilistically, suppose the enemy has ten army units and one scout, we are likely to choose areas of importance (an army unit) but not to disregard attacking a scout entirely.

We found that this approach worked well because overall it chooses to attack &quot;correct&quot; areas seeing as how the enemy base is generally filled with enemy units. On the other hand, when our bot was short on targets often attacking some seemly random point was helpful. For example, our bot attacked some far-off point once and uncovered an undiscovered enemy expansion once.

**Future Work**

Our bot could be expanded in a couple of ways:

- Game Strategy
  - Implementing multiple strategies would be interesting to mimic human play and to make playing against the bot more entertaining by adding variation.
- Refine Knowledge Representation Methods
  - Given more time, we would be interested in developing a better system to manage information for video game bots. This alone could likely we a huge module focusing on some mix of Cognitive Science and Software Engineering patterns to implement an elegant system to represent in game knowledge.
- Implement more Terran specific features
  - In our progress reports we list some factional abilities and mechanics we&#39;d like to look at implementing. They are the use of a dynamic marine-marauder unit composition, the use of stim packs, and the use of medical dropships. We decided against the first two because we felt that they relied too much on game knowledge which tied too much into knowledge representation. Inferring the enemy&#39;s unit composition is tricky and dangerous is wrong similarity for using the stim ability.
- Implement cloak detection.
  - Cloaked units are an area not covered by our bot design. Given more time, we could experiment with simple heuristics and detector units/builds to counter strategies using cloaked units.

**Appendix: Software Modules**
