/*
 *  File:       acr.cc
 *  Summary:    Main entry point, event loop, and some initialization functions
 *  Written by: Linley Henzell
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 *
 *  Change History (most recent first):
 *
 * <18> 7/29/00         JDJ             values.h isn't included on Macs
 * <17> 19jun2000       GDL             added Windows console support
 * <16> 06mar2000       bwr             changes to berserk
 * <15> 09jan2000       BCR             new Wiz command: blink
 * <14> 10/13/99        BCR             Added auto door opening,
 *                                       move "you have no
 *                                       religion" to describe.cc
 * <13> 10/11/99        BCR             Added Daniel's wizard patch
 * <12> 10/9/99         BCR             swapped 'v' and 'V' commands,
 *                                       added wizard help command
 * <11> 10/7/99         BCR             haste and slow now take amulet of
 *                                       resist slow into account
 * <10> 9/25/99         CDL             Changes to Linux input
 *                                       switch on command enums
 * <9>  6/12/99         BWR             New init code, restructured
 *                                       wiz commands, added equipment
 *                                       listing commands
 * <8>   6/7/99         DML             Autopickup
 * <7>  5/30/99         JDJ             Added game_has_started.
 * <6>  5/25/99         BWR             Changed move() to move_player()
 * <5>  5/20/99         BWR             New berserk code, checking
 *                                       scan_randarts for NO_TELEPORT
 *                                       and NO_SPELLCASTING.
 * <4>  5/12/99         BWR             Solaris support.
 * <3>  5/09/99         JDJ             look_around no longer prints a prompt.
 * <2>  5/08/99         JDJ             you and env are no longer arrays.
 * <1>  -/--/--         LRH             Created
 */

#include "AppHdr.h"

#include <string>

// I don't seem to need values.h for VACPP..
#if !defined(__IBMCPP__)
#include <limits.h>
#endif

#if DEBUG
  // this contains the DBL_MAX constant
  #include <float.h>
#endif

#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <stdio.h>

#ifdef DOS
#include <dos.h>
#include <conio.h>
#include <file.h>
#endif

#ifdef USE_UNIX_SIGNALS
#include <signal.h>
#endif

#include "externs.h"

#include "abl-show.h"
#include "abyss.h"
#include "branch.h"
#include "chardump.h"
#include "cloud.h"
#include "clua.h"
#include "command.h"
#include "debug.h"
#include "delay.h"
#include "describe.h"
#include "direct.h"
#include "dungeon.h"
#include "effects.h"
#include "fight.h"
#include "files.h"
#include "food.h"
#include "hiscores.h"
#include "initfile.h"
#include "invent.h"
#include "item_use.h"
#include "it_use2.h"
#include "it_use3.h"
#include "itemname.h"
#include "itemprop.h"
#include "items.h"
#include "lev-pand.h"
#include "macro.h"
#include "maps.h"
#include "misc.h"
#include "monplace.h"
#include "monstuff.h"
#include "mon-util.h"
#include "mutation.h"
#include "newgame.h"
#include "notes.h"
#include "ouch.h"
#include "output.h"
#include "overmap.h"
#include "player.h"
#include "randart.h"
#include "religion.h"
#include "shopping.h"
#include "skills.h"
#include "skills2.h"
#include "spells1.h"
#include "spells3.h"
#include "spells4.h"
#include "spl-book.h"
#include "spl-cast.h"
#include "spl-util.h"
#include "stuff.h"
#include "tags.h"
#include "transfor.h"
#include "travel.h"
#include "view.h"
#include "stash.h"

crawl_environment env;
player you;
system_environment SysEnv;
game_state crawl_state;

std::string init_file_location; // externed in newgame.cc

char info[ INFO_SIZE ];         // messaging queue extern'd everywhere {dlb}

int stealth;                    // externed in view.cc
char use_colour = 1;

bool just_autoprayed = false;
bool about_to_autopray = false;

// set to true once a new game starts or an old game loads
bool game_has_started = false;

// Clockwise, around the compass from north (same order as enum RUN_DIR)
const struct coord_def Compass[8] = 
{ 
    coord_def(0, -1), coord_def(1, -1), coord_def(1, 0), coord_def(1, 1),
    coord_def(0, 1), coord_def(-1, 1), coord_def(-1, 0), coord_def(-1, -1),
};

// Functions in main module
static void close_door(int move_x, int move_y);
static void do_berserk_no_combat_penalty(void);
static bool initialise(void);
static void input(void);
static void move_player(int move_x, int move_y);
static void open_door(int move_x, int move_y, bool check_confused = true);
static void start_running( int dir, int mode );
static void close_door(int move_x, int move_y);

static void init_io();
static void prep_input();
static void input();
static void middle_input();
static void world_reacts();
static command_type get_next_cmd();
static keycode_type get_next_keycode();
static command_type keycode_to_command( keycode_type key );

/*
   It all starts here. Some initialisations are run first, then straight to
   new_game and then input.
*/
int main( int argc, char *argv[] )
{
    // Load in the system environment variables
    get_system_environment();

    // parse command line args -- look only for initfile & crawl_dir entries
    if (!parse_args(argc, argv, true))
    {
        // print help
        puts("Command line options:");
        puts("  -name <string>   character name");
        puts("  -race <arg>      preselect race (by letter, abbreviation, or name)");
        puts("  -class <arg>     preselect class (by letter, abbreviation, or name)");
        puts("  -pizza <string>  crawl pizza");
        puts("  -plain           don't use IBM extended characters");
        puts("  -dir <path>      crawl directory");
        puts("  -rc <file>       init file name");
        puts("  -morgue <dir>    directory to save character dumps");
        puts("  -macro <file>    macro file name");
        puts("");
        puts("Command line options override init file options, which override");
        puts("environment options (CRAWL_NAME, CRAWL_PIZZA, CRAWL_DIR, CRAWL_RC).");
        puts("");
        puts("Highscore list options: (Can now be redirected to more, etc)");
        puts("  -scores [N]            highscore list");
        puts("  -tscores [N]           terse highscore list");
        puts("  -vscores [N]           verbose highscore list");
        puts("  -scorefile <filename>  scorefile to report on");
        exit(1);
    }

    // Read the init file
    init_file_location = read_init_file();

    // now parse the args again, looking for everything else.
    parse_args( argc, argv, false );

    if (Options.sc_entries != 0)
    {
        hiscores_print_all( Options.sc_entries, Options.sc_format );
        exit(0);
    }
    else
    {
        // Don't allow scorefile override for actual gameplay, only for
        // score listings.
        SysEnv.scorefile.clear();
    }

    bool game_start = initialise();

    if (game_start || Options.always_greet)
    {
        snprintf( info, INFO_SIZE, "Welcome, %s the %s %s.", 
                  you.your_name, species_name( you.species,you.experience_level ), you.class_name );

        mpr( info );

        // Starting messages can go here as this should only happen
        // at the start of a new game -- bwr
        // This message isn't appropriate for Options.always_greet
        if (you.char_class == JOB_WANDERER && game_start)
        {
            int skill_levels = 0;
            for (int i = 0; i <= NUM_SKILLS; i++)
                skill_levels += you.skills[ i ];

            if (skill_levels <= 2)
            {
                // Demigods and Demonspawn wanderers stand to not be
                // able to see any of their skills at the start of
                // the game (one or two skills should be easily guessed
                // from starting equipment)... Anyways, we'll give the
                // player a message to warn them (and give a reason why). -- bwr
                mpr("You wake up in a daze, and can't recall much.");
            }
        }

        // These need some work -- should make sure that the god's
        // name is metioned, else the message might be confusing.
        switch (you.religion)
        {
        case GOD_ZIN:
            simple_god_message( " says: Spread the light, my child." );
            break;
        case GOD_SHINING_ONE:
            simple_god_message( " says: Smite the infidels!" );
            break;
        case GOD_KIKUBAAQUDGHA:
        case GOD_YREDELEMNUL:
        case GOD_NEMELEX_XOBEH:
            simple_god_message( " says: Welcome..." );
            break;
        case GOD_XOM:
            if (game_start)
                simple_god_message( " says: A new plaything!" );
            break;
        case GOD_VEHUMET:
            god_speaks( you.religion, "Let it end in hellfire!");
            break;
        case GOD_OKAWARU:
            simple_god_message(" says: Welcome, disciple.");
            break;
        case GOD_MAKHLEB:
            god_speaks( you.religion, "Blood and souls for Makhleb!" );
            break;
        case GOD_SIF_MUNA:
            simple_god_message( " whispers: I know many secrets...");
            break;
        case GOD_TROG:
            simple_god_message( " says: Kill them all!" );
            break;
        case GOD_ELYVILON:
            simple_god_message( " says: Go forth and aid the weak!" );
            break;
        default:
            break;
        }

        // warn player about their weapon, if unsuitable
        wield_warning(false);
    }

    if ( game_start )
    {
        snprintf(info, INFO_SIZE,
                 "%s, the %s %s, began the quest for the Orb.",
                 you.your_name,
                 species_name(you.species,you.experience_level),
                 you.class_name);                 
        take_note(Note(NOTE_USER_NOTE, 0, 0, info));
        snprintf(info, INFO_SIZE, "HP: %d/%d MP: %d/%d",
                 you.hp, you.hp_max, you.magic_points, you.max_magic_points);
        take_note(Note(NOTE_XP_LEVEL_CHANGE, you.experience_level, 0, info));
    }

    while (true)
    {
        input();
        //      cprintf("x");
    }

    // Should never reach this stage, right?

#ifdef UNIX
    unixcurses_shutdown();
#endif

    return 0;
}                               // end main()

static void init_io()
{
#ifdef UNIX
    unixcurses_startup();
#endif

#ifdef WIN32CONSOLE
    init_libw32c();
#endif

#ifdef DOS
    init_libdos();
#endif
}

#ifdef WIZARD
static void handle_wizard_command( void )
{
    int   wiz_command, i, j, tmp;
    char  specs[256];

    // WIZ_NEVER gives protection for those who have wiz compiles,
    // and don't want to risk their characters.
    if (Options.wiz_mode == WIZ_NEVER)
        return;

    if (!you.wizard)
    {
        mpr( "WARNING: ABOUT TO ENTER WIZARD MODE!", MSGCH_WARN );

#ifndef SCORE_WIZARD_MODE
        mpr( "If you continue, your game will not be scored!", MSGCH_WARN );
#endif

        if (!yesno( "Do you really want to enter wizard mode?", 
                    false, 'n' ))
            return;

        you.wizard = true;
        redraw_screen();
    }

    mpr( "Enter Wizard Command: ", MSGCH_PROMPT );
    wiz_command = getch();

    switch (wiz_command)
    { 
    case '?':
        list_commands(true);        // tell it to list wizard commands
        redraw_screen();
        break;

    case CONTROL('G'):
        save_ghost(true);
        break; 

    case 'x':
        you.experience = 1 + exp_needed( 2 + you.experience_level );
        level_change();
        break;

    case 's':
        you.exp_available = 20000;
        you.redraw_experience = 1;
        break;

    case 'S':
        debug_set_skills();
        break;

    case 'A':
        debug_set_all_skills();
        break;

    case '$':
        you.gold += 1000;
        you.redraw_gold = 1;
        break;

    case 'a':
        acquirement( OBJ_RANDOM, AQ_WIZMODE );
        break;

    case 'v':
        // this command isn't very exciting... feel free to replace
        i = prompt_invent_item( "Value of which item?", MT_INVSELECT, -1 );
        if (i == PROMPT_ABORT || !is_random_artefact( you.inv[i] ))
        {
            canned_msg( MSG_OK );
            break;
        }
        else
        {
            snprintf( info, INFO_SIZE, "randart val: %d", randart_value( you.inv[i] ) ); 
            mpr( info );
        }
        break;

    case '+':
        i = prompt_invent_item(
                "Make an artefact out of which item?", MT_INVSELECT, -1 );
        if (i == PROMPT_ABORT)
        {
            canned_msg( MSG_OK );
            break;
        }

        // set j == equipment slot of chosen item, remove old randart benefits
        for (j = 0; j < NUM_EQUIP; j++)
        {
            if (you.equip[j] == i)
            {
                if (j == EQ_WEAPON)
                    you.wield_change = true;

                if (is_random_artefact( you.inv[i] ))
                    unuse_randart( i );

                break;
            }
        }

        make_item_randart( you.inv[i] );

        // if equiped, apply new randart benefits
        if (j != NUM_EQUIP)
            use_randart( i );

        item_name( you.inv[i], DESC_INVENTORY_EQUIP, info );
        mpr( info );
        break;

    case '|':
        // create all unrand arts
        for (tmp = 1; tmp < NO_UNRANDARTS; tmp++)
        {
            int islot = get_item_slot();
            if (islot == NON_ITEM)
                break;

            make_item_unrandart( mitm[islot], tmp );

            mitm[ islot ].quantity = 1;
            set_ident_flags( mitm[ islot ], ISFLAG_IDENT_MASK );

            move_item_to_grid( &islot, you.x_pos, you.y_pos );
        }

        // create all fixed artefacts
        for (tmp = SPWPN_SINGING_SWORD; tmp <= SPWPN_STAFF_OF_WUCAD_MU; tmp++) 
        {
            int islot = get_item_slot();
            if (islot == NON_ITEM)
                break;

            if (make_item_fixed_artefact( mitm[ islot ], false, tmp ))
            {
                mitm[ islot ].quantity = 1;
                item_colour( mitm[ islot ] );
                set_ident_flags( mitm[ islot ], ISFLAG_IDENT_MASK );

                move_item_to_grid( &islot, you.x_pos, you.y_pos );
            }
        }
        break;

    case 'B':
        if (you.level_type != LEVEL_ABYSS)
            banished( DNGN_ENTER_ABYSS );
        else
        {
            down_stairs(true, you.your_level, DNGN_EXIT_ABYSS);
            untag_followers();
        }
        break;

    case 'g':
        debug_add_skills();
        break;

    case 'G':
        // Genocide... "unsummon" all the monsters from the level.
        for (int mon = 0; mon < MAX_MONSTERS; mon++)
        {
            struct monsters *monster = &menv[mon];

            if (monster->type == -1)
                continue;

            monster_die(monster, KILL_RESET, 0);

        }
        break;

    case 'h':
        you.rotting = 0;
        you.poison = 0;
        you.disease = 0;
        set_hp( abs(you.hp_max), false );
        set_hunger( 5000 + abs(you.hunger), true );
        break;

    case 'H':
        you.rotting = 0;
        you.poison = 0;
        you.disease = 0;
        inc_hp( 10, true );
        set_hp( you.hp_max, false );
        set_hunger( 12000, true );
        you.redraw_hit_points = 1;
        break;

    case 'b':
        blink();            // wizards can always blink
        break;

    case '\"':
    case '~':
        level_travel(0);
        break;

    case 'd':
    case 'D':
        level_travel(1);
        break;

    case 'u':
    case 'U':
        level_travel(-1);
        break;

    case '%':
    case 'o':
        create_spec_object();
        break;

    case 't':
        tweak_object();
        break;

    case 'T':
        debug_make_trap();
        break;

    case '\\':
        debug_make_shop();
        break;

    case 'f':
        debug_fight_statistics(false);
        break;

    case 'F':
        debug_fight_statistics(true);
        break;

    case 'm':
        create_spec_monster();
        break;

    case 'M':
        create_spec_monster_name();
        break;

    case 'r':
        debug_change_species();
        break;

    case '>':
        grd[you.x_pos][you.y_pos] = DNGN_STONE_STAIRS_DOWN_I;
        break;

    case '<':
        grd[you.x_pos][you.y_pos] = DNGN_ROCK_STAIRS_UP;
        break;

    case 'p':
        grd[you.x_pos][you.y_pos] = DNGN_ENTER_PANDEMONIUM;
        break;

    case 'l':
        grd[you.x_pos][you.y_pos] = DNGN_ENTER_LABYRINTH;
        break;

    case 'i':
        mpr( "You feel a rush of knowledge." );
        for (i = 0; i < ENDOFPACK; i++)
        {
            if (is_valid_item( you.inv[i] ))
            {
                set_ident_type( you.inv[i].base_type, you.inv[i].sub_type, 
                                ID_KNOWN_TYPE );

                set_ident_flags( you.inv[i], ISFLAG_IDENT_MASK );
            }
        }
        you.wield_change = true;
        break;

    case 'I':
        mpr( "You feel a rush of antiknowledge." );
        for (i = 0; i < ENDOFPACK; i++)
        {
            if (is_valid_item( you.inv[i] ))
            {
                set_ident_type( you.inv[i].base_type, you.inv[i].sub_type, 
                                ID_UNKNOWN_TYPE );

                unset_ident_flags( you.inv[i], ISFLAG_IDENT_MASK );
            }
        }
        you.wield_change = true;
        break;

    case 'X':
        Xom_acts(true, 20, true);
        break;

    case 'z':
        cast_spec_spell();
        break;              /* cast spell by number */

    case 'Z':
        cast_spec_spell_name();
        break;              /* cast spell by name */

    case '(':
        mpr( "Create which feature (by number)? ", MSGCH_PROMPT );
        get_input_line( specs, sizeof( specs ) );

        if (specs[0] != '\0')
            grd[you.x_pos][you.y_pos] = atoi(specs);
        break;

    case ']':
        if (!debug_add_mutation())
            mpr( "Failure to give mutation." );
        break;

    case '[':
        demonspawn();
        break;

    case ':':
        j = 0;
        for (i = 0; i < NUM_BRANCHES; i++)
            if (branches[i].startdepth != - 1)
                mprf(MSGCH_DIAGNOSTICS, "Branch %d (%s) is on level %d of %s",
                     i, branches[i].longname, branches[i].startdepth,
                     branches[branches[i].parent_branch].abbrevname);
        break;

    case '{':
        magic_mapping(1000, 100);
        break;

    case '@':
        debug_set_stats();
        break;

    case '^':
        {
            int old_piety = you.piety;

            gain_piety(50);
            snprintf( info, INFO_SIZE, "Congratulations, your piety went from %d to %d!",
                    old_piety, you.piety);
            mpr(info);
        }
        break;

    case '=':
        snprintf( info, INFO_SIZE, 
                  "Cost level: %d  Skill points: %d  Next cost level: %d", 
                  you.skill_cost_level, you.total_skill_points,
                  skill_cost_needed( you.skill_cost_level + 1 ) );

        mpr( info );
        break;

    case '_':
        debug_get_religion();
        break;

    case '\'':
        for (i = 0; i < MAX_ITEMS; i++)
        {
            if (mitm[i].link == NON_ITEM)
                continue;
    
            snprintf( info, INFO_SIZE, "item:%3d link:%3d cl:%3d ty:%3d pl:%3d pl2:%3d sp:%3ld q:%3d",
                     i, mitm[i].link, 
                     mitm[i].base_type, mitm[i].sub_type,
                     mitm[i].plus, mitm[i].plus2, mitm[i].special, 
                     mitm[i].quantity );

            mpr(info);
        }

        strcpy(info, "igrid:");
        mpr(info);

        for (i = 0; i < GXM; i++)
        {
            for (j = 0; j < GYM; j++)
            {
                if (igrd[i][j] != NON_ITEM)
                {
                    snprintf( info, INFO_SIZE, "%3d at (%2d,%2d), cl:%3d ty:%3d pl:%3d pl2:%3d sp:%3ld q:%3d", 
                             igrd[i][j], i, j,
                             mitm[i].base_type, mitm[i].sub_type,
                             mitm[i].plus, mitm[i].plus2, mitm[i].special, 
                             mitm[i].quantity );

                    mpr(info);
                }
            }
        }
        break;

    default:
        mpr("Not a Wizard Command.");
        break;
    }
}
#endif

// Set up the running variables for the current run.
static void start_running( int dir, int mode )
{
    if (i_feel_safe(true))
        you.running.initialise(dir, mode);
}

static bool recharge_rod( item_def &rod, bool wielded )
{
    if (!item_is_rod(rod) || rod.plus >= rod.plus2 || !enough_mp(1, true))
        return (false);

    const int charge = rod.plus / ROD_CHARGE_MULT;

    int rate = ((charge + 1) * ROD_CHARGE_MULT) / 10;
            
    rate *= (10 + skill_bump( SK_EVOCATIONS ));
    rate = div_rand_round( rate, 100 );

    if (rate < 5)
        rate = 5;
    else if (rate > ROD_CHARGE_MULT / 2)
        rate = ROD_CHARGE_MULT / 2;

    // If not wielded, the rod charges far more slowly.
    if (!wielded)
        rate /= 5;
    // Shields hamper recharging for wielded rods.
    else if (player_shield())
        rate /= 2;

    if (rod.plus / ROD_CHARGE_MULT != (rod.plus + rate) / ROD_CHARGE_MULT)
    {
        dec_mp(1);
        if (wielded)
            you.wield_change = true;
    }

    rod.plus += rate;
    if (rod.plus > rod.plus2)
        rod.plus = rod.plus2;

    if (wielded && rod.plus == rod.plus2 && is_resting())
        stop_running();

    return (true);
}

static void recharge_rods()
{
    const int wielded = you.equip[EQ_WEAPON];
    if (wielded != -1)
    {
        if (recharge_rod( you.inv[wielded], true ))
            return ;
    }

    for (int i = 0; i < ENDOFPACK; ++i)
    {
        if (i != wielded && is_valid_item(you.inv[i])
                && one_chance_in(3)
                && recharge_rod( you.inv[i], false ))
            return;
    }
}

/* used to determine whether to apply the berserk penalty at end
   of round */
bool apply_berserk_penalty = false;

/*
  This function handles the player's input. It's called from main(), from
  inside an endless loop.
 */
static void input() {

    you.turn_is_over = false;
    prep_input();

    if ( you.paralysis )
    {
        world_reacts();
        return;
    }
    
    middle_input();

    if ( need_to_autopickup() )
        autopickup();

    handle_delay();

    gotoxy(18,9);

    if ( you_are_delayed() )
    {
        world_reacts();
        return;
    }

    if ( you.turn_is_over )
    {
        world_reacts();
        return;
    }

    {
        // Enable the cursor to read input. The cursor stays on while
        // the command is being processed, so subsidiary prompts
        // shouldn't need to turn it on explicitly.
        cursor_control con(true);
        command_type cmd = get_next_cmd();

        // [dshaligram] If get_next_cmd encountered a Lua macro
        // binding, your turn may be ended by the first invoke of the
        // macro.
        if (!you.turn_is_over && cmd != CMD_NEXT_CMD)
            process_command( cmd );
    }

    if (you.turn_is_over)
    {
        if ( apply_berserk_penalty )
            do_berserk_no_combat_penalty();

        world_reacts();
    }
    else
        viewwindow(true, false);
}

static int toggle_flag( bool* flag, const char* flagname ) {
    char buf[INFO_SIZE];
    *flag = !(*flag);
    sprintf( buf, "%s is now %s.", flagname,
             (*flag) ? "on" : "off" );
    mpr(buf);
    return *flag;
}

static void go_upstairs() {
    if (grd[you.x_pos][you.y_pos] == DNGN_ENTER_SHOP)
    {
        if ( you.berserker )
            canned_msg(MSG_TOO_BERSERK);
        else
            shop();
        return;
    }
    else if ((grd[you.x_pos][you.y_pos] < DNGN_STONE_STAIRS_UP_I
              || grd[you.x_pos][you.y_pos] > DNGN_ROCK_STAIRS_UP)
             && (grd[you.x_pos][you.y_pos] < DNGN_RETURN_FROM_ORCISH_MINES 
                 || grd[you.x_pos][you.y_pos] >= 150)) {   
        mpr( "You can't go up here!" );
        return;
    }

    tag_followers();  // only those beside us right now can follow
    start_delay( DELAY_ASCENDING_STAIRS, 
                 1 + (you.burden_state > BS_UNENCUMBERED) );
}

static void go_downstairs() {

    if ((grd[you.x_pos][you.y_pos] < DNGN_ENTER_LABYRINTH
         || grd[you.x_pos][you.y_pos] > DNGN_ROCK_STAIRS_DOWN)
        && grd[you.x_pos][you.y_pos] != DNGN_ENTER_HELL
        && ((grd[you.x_pos][you.y_pos] < DNGN_ENTER_DIS
             || grd[you.x_pos][you.y_pos] > DNGN_TRANSIT_PANDEMONIUM)
            && grd[you.x_pos][you.y_pos] != DNGN_STONE_ARCH)
        && !(grd[you.x_pos][you.y_pos] >= DNGN_ENTER_ORCISH_MINES
             && grd[you.x_pos][you.y_pos] < DNGN_RETURN_FROM_ORCISH_MINES)) {
        mpr( "You can't go down here!" );
        return;
    }

    tag_followers();  // only those beside us right now can follow
    start_delay( DELAY_DESCENDING_STAIRS,
                 1 + (you.burden_state > BS_UNENCUMBERED),
                 you.your_level );
}

static void experience_check() {
    snprintf( info, INFO_SIZE, "You are a level %d %s %s.",
              you.experience_level,
              species_name(you.species,you.experience_level),
              you.class_name);
    mpr(info);

    if (you.experience_level < 27) {
        int xp_needed = (exp_needed(you.experience_level+2)-you.experience)+1;
        snprintf( info, INFO_SIZE,
                  "Level %d requires %ld experience (%d point%s to go!)",
                  you.experience_level + 1, 
                  exp_needed(you.experience_level + 2) + 1,
                  xp_needed, 
                  (xp_needed > 1) ? "s" : "");
        mpr(info);
    }
    else {
        mpr( "I'm sorry, level 27 is as high as you can go." );
        mpr( "With the way you've been playing, I'm surprised you got this far." );
    }

    if (you.real_time != -1) {
        const time_t curr = you.real_time + (time(NULL) - you.start_time);
        char buff[200];

        make_time_string( curr, buff, sizeof(buff) );

        snprintf( info, INFO_SIZE, "Play time: %s (%ld turns)", 
                  buff, you.num_turns );

        mpr( info );
    }
#ifdef DEBUG_DIAGNOSTICS
    if (wearing_amulet(AMU_THE_GOURMAND))
        mprf(MSGCH_DIAGNOSTICS, "Gourmand charge: %d", 
             you.duration[DUR_GOURMAND]);
#endif
}

/* note that in some actions, you don't want to clear afterwards.
   e.g. list_jewellery, etc. */

void process_command( command_type cmd ) {

    FixedVector < int, 2 > plox;
    apply_berserk_penalty = true;

    switch ( cmd ) {

    case CMD_OPEN_DOOR_UP_RIGHT:   open_door(-1, -1); break;
    case CMD_OPEN_DOOR_UP:         open_door( 0, -1); break;
    case CMD_OPEN_DOOR_UP_LEFT:    open_door( 1, -1); break;
    case CMD_OPEN_DOOR_RIGHT:      open_door( 1,  0); break;
    case CMD_OPEN_DOOR_DOWN_RIGHT: open_door( 1,  1); break;
    case CMD_OPEN_DOOR_DOWN:       open_door( 0,  1); break;
    case CMD_OPEN_DOOR_DOWN_LEFT:  open_door(-1,  1); break;
    case CMD_OPEN_DOOR_LEFT:       open_door(-1,  0); break;

    case CMD_MOVE_DOWN_LEFT:  move_player(-1,  1); break;
    case CMD_MOVE_DOWN:       move_player( 0,  1); break;
    case CMD_MOVE_UP_RIGHT:   move_player( 1, -1); break;
    case CMD_MOVE_UP:         move_player( 0, -1); break;
    case CMD_MOVE_UP_LEFT:    move_player(-1, -1); break;
    case CMD_MOVE_LEFT:       move_player(-1,  0); break;
    case CMD_MOVE_DOWN_RIGHT: move_player( 1,  1); break;
    case CMD_MOVE_RIGHT:      move_player( 1,  0); break;

    case CMD_REST:
        if (i_feel_safe())
        {
            if ( you.hp == you.hp_max &&
                 you.magic_points == you.max_magic_points )
                mpr("You start searching.");
            else
                mpr("You start resting.");
        }
        start_running( RDIR_REST, RMODE_REST_DURATION );
        break;

    case CMD_RUN_DOWN_LEFT:
        start_running( RDIR_DOWN_LEFT, RMODE_START );
        break;
    case CMD_RUN_DOWN:
        start_running( RDIR_DOWN, RMODE_START );
        break;
    case CMD_RUN_UP_RIGHT:
        start_running( RDIR_UP_RIGHT, RMODE_START );
        break;
    case CMD_RUN_UP:
        start_running( RDIR_UP, RMODE_START );
        break;
    case CMD_RUN_UP_LEFT:
        start_running( RDIR_UP_LEFT, RMODE_START );
        break;
    case CMD_RUN_LEFT:
        start_running( RDIR_LEFT, RMODE_START );
        break;
    case CMD_RUN_DOWN_RIGHT:
        start_running( RDIR_DOWN_RIGHT, RMODE_START );
        break;
    case CMD_RUN_RIGHT:
        start_running( RDIR_RIGHT, RMODE_START );
        break;

    case CMD_TOGGLE_AUTOPICKUP:
        toggle_flag( &Options.autopickup_on, "Autopickup");
        break;

    case CMD_TOGGLE_AUTOPRAYER:
        toggle_flag( &Options.autoprayer_on, "Autoprayer" );
        break;

    case CMD_TOGGLE_NOFIZZLE:
        toggle_flag( &Options.fizzlecheck_on, "Fizzle confirmation" );
        break;
    
    case CMD_MAKE_NOTE:
        make_user_note();
        break;

    case CMD_DISPLAY_NOTES:
        display_notes();
        break;

    case CMD_CLEAR_MAP:
        if (you.level_type != LEVEL_LABYRINTH &&
            you.level_type != LEVEL_ABYSS) {
            mpr("Clearing level map.");
            clear_map();
        }
        break;

    case CMD_GO_UPSTAIRS: go_upstairs(); break;
    case CMD_GO_DOWNSTAIRS: go_downstairs(); break;
    case CMD_DISPLAY_OVERMAP: display_overmap(); break;
    case CMD_OPEN_DOOR: open_door(0, 0); break;
    case CMD_CLOSE_DOOR: close_door(0, 0); break;

    case CMD_DROP:
        drop();
        if (Options.stash_tracking >= STM_DROPPED)
            stashes.add_stash();
        break;
        
    case CMD_SEARCH_STASHES:
        stashes.search_stashes();
        break;

    case CMD_MARK_STASH:
        if (Options.stash_tracking >= STM_EXPLICIT)
            stashes.add_stash(-1, -1, true);
        break;

    case CMD_FORGET_STASH:
        if (Options.stash_tracking >= STM_EXPLICIT)
            stashes.no_stash();
        break;

    case CMD_BUTCHER:
        butchery();
        break;

    case CMD_DISPLAY_INVENTORY:
        get_invent(-1);
        break;

    case CMD_EVOKE:
        if (!evoke_wielded())
            flush_input_buffer( FLUSH_ON_FAILURE );
        break;

    case CMD_PICKUP:
        pickup();
        break;

    case CMD_INSPECT_FLOOR:
        item_check(';');
        break;

    case CMD_WIELD_WEAPON:
        wield_weapon(false);
        break;

    case CMD_THROW:
        throw_anything();
        break;

    case CMD_FIRE:
        shoot_thing();
        break;

    case CMD_WEAR_ARMOUR:
        wear_armour();
        break;

    case CMD_REMOVE_ARMOUR:
    {
        int index=0;

        if (armour_prompt("Take off which item?", &index, OPER_TAKEOFF))
            takeoff_armour(index);
    }
    break;

    case CMD_REMOVE_JEWELLERY:
        remove_ring();
        break;

    case CMD_WEAR_JEWELLERY:
        puton_ring(-1, false);
        break;

    case CMD_ADJUST_INVENTORY:
        adjust();
        break;

    case CMD_MEMORISE_SPELL:
        if (!learn_spell())
            flush_input_buffer( FLUSH_ON_FAILURE );
        break;

    case CMD_ZAP_WAND:
        zap_wand();
        break;

    case CMD_EAT:
        eat_food();
        break;

    case CMD_USE_ABILITY:
        if (!activate_ability())
            flush_input_buffer( FLUSH_ON_FAILURE );
        break;

    case CMD_DISPLAY_MUTATIONS:
        display_mutations();
        redraw_screen();
        break;

    case CMD_EXAMINE_OBJECT:
        examine_object();
        break;

    case CMD_PRAY:
        pray();
        break;

    case CMD_DISPLAY_RELIGION:
        describe_god( you.religion, true );
        redraw_screen();
        break;

    case CMD_MOVE_NOWHERE:
    case CMD_SEARCH:
        search_around();
        you.turn_is_over = true;
        break;

    case CMD_QUAFF:
        drink();
        break;

    case CMD_READ:
        read_scroll();
        break;

    case CMD_LOOK_AROUND:
        mpr("Move the cursor around to observe a square.", MSGCH_PROMPT);
        mpr("Press '?' for a monster description.", MSGCH_PROMPT);

        struct dist lmove;
        lmove.isValid = lmove.isTarget = lmove.isCancel = false;
        look_around( lmove, true );
        if (lmove.isValid && lmove.isTarget && !lmove.isCancel)
            start_travel( lmove.tx, lmove.ty );
        break;

    case CMD_CAST_SPELL:
        /* randart wpns */
        if (scan_randarts(RAP_PREVENT_SPELLCASTING))
        {
            mpr("Something interferes with your magic!");
            flush_input_buffer( FLUSH_ON_FAILURE );
            break;
        }

        if (!cast_a_spell())
            flush_input_buffer( FLUSH_ON_FAILURE );
        break;

    case CMD_WEAPON_SWAP:
        wield_weapon(true);
        break;

        // [ds] Waypoints can be added from the level-map, and we need
        // Ctrl+F for nobler things. Who uses waypoints, anyway?
        // Update: Appears people do use waypoints. Reinstating, on
        // CONTROL('W'). This means Ctrl+W is no longer a wizmode
        // trigger, but there's always '&'. :-)
    case CMD_FIX_WAYPOINT:
        travel_cache.add_waypoint();
        break;
        
    case CMD_INTERLEVEL_TRAVEL:
        if (!can_travel_interlevel())
        {
            mpr("Sorry, you can't auto-travel out of here.");
            break;
        }
        start_translevel_travel();
        if (you.running)
            mesclr();
        break;

    case CMD_EXPLORE:
        if (you.level_type == LEVEL_LABYRINTH || you.level_type == LEVEL_ABYSS)
        {
            mpr("It would help if you knew where you were, first.");
            break;
        }
        // Start exploring
        start_explore(Options.explore_greedy);
        break;

    case CMD_DISPLAY_MAP:
#if (!DEBUG_DIAGNOSTICS)
        if (you.level_type == LEVEL_LABYRINTH || you.level_type == LEVEL_ABYSS)
        {
            mpr("It would help if you knew where you were, first.");
            break;
        }
#endif
        plox[0] = 0;
        show_map(plox, true);
        redraw_screen();
        if (plox[0] > 0)
            start_travel(plox[0], plox[1]);
        break;

    case CMD_DISPLAY_KNOWN_OBJECTS:
        check_item_knowledge();
        break;

#ifdef ALLOW_DESTROY_ITEM_COMMAND
    case CMD_DESTROY_ITEM:
        cmd_destroy_item();
        break;
#endif

    case CMD_REPLAY_MESSAGES:
        replay_messages();
        redraw_screen();
        break;

    case CMD_REDRAW_SCREEN:
        redraw_screen();
        break;

    case CMD_SAVE_GAME_NOW:
        mpr("Saving game... please wait.");
        save_game(true);
        break;

#ifdef USE_UNIX_SIGNALS
    case CMD_SUSPEND_GAME:
        // CTRL-Z suspend behaviour is implemented here,
        // because we want to have CTRL-Y available...
        // and unfortunately they tend to be stuck together. 
        clrscr();
        unixcurses_shutdown();
        kill(0, SIGTSTP);
        unixcurses_startup();
        redraw_screen();
        break;
#endif

    case CMD_DISPLAY_COMMANDS:
        list_commands(false);
        redraw_screen();
        break;

    case CMD_EXPERIENCE_CHECK:
        experience_check();
        break;

    case CMD_SHOUT:
        yell();                 /* in effects.cc */
        break;

    case CMD_DISPLAY_CHARACTER_STATUS:
        display_char_status();
        break;
        
    case CMD_RESISTS_SCREEN:
        resists_screen();
        break;

    case CMD_DISPLAY_SKILLS:
        show_skills();
        redraw_screen();
        break;

    case CMD_CHARACTER_DUMP:
        char name_your[kNameLen+1];

        strncpy(name_your, you.your_name, kNameLen);
        name_your[kNameLen] = 0;
        if (dump_char( name_your, false ))
            mpr("Char dumped successfully.");
        else
            mpr("Char dump unsuccessful! Sorry about that.");
        break;

#ifdef USE_MACROS
    case CMD_MACRO_ADD:
        macro_add_query();
        break;
#endif

    case CMD_LIST_WEAPONS:
        list_weapons();
        break;

    case CMD_INSCRIBE_ITEM:
        inscribe_item();
        break;
        
    case CMD_LIST_ARMOUR:
        list_armour();
        break;

    case CMD_LIST_JEWELLERY:
        list_jewellery();
        break;

#ifdef WIZARD
    case CMD_WIZARD:
        handle_wizard_command();
        break;
#endif

    case CMD_SAVE_GAME:
        if (yesno("Save game and exit?", true, 'n'))
            save_game(true);
        break;

    case CMD_QUIT:
        quit_game();
        break;

    case CMD_GET_VERSION:
        version();
        break;

    case CMD_NO_CMD:
    default:
        mpr("Unknown command.");
        break;

    }
}

static void prep_input() {
    you.time_taken = player_speed();
    you.shield_blocks = 0;              // no blocks this round
#ifdef UNIX
    update_screen();
#else
    window( 1, 1, 80, get_number_of_lines() );
#endif

    textcolor(LIGHTGREY);

    set_redraw_status( REDRAW_LINE_2_MASK | REDRAW_LINE_3_MASK );
    print_stats();
}

static void decrement_durations()
{
    if (wearing_amulet(AMU_THE_GOURMAND))
    {
        if (you.duration[DUR_GOURMAND] < GOURMAND_MAX && one_chance_in(4))
            you.duration[DUR_GOURMAND]++;
    }
    else
        you.duration[DUR_GOURMAND] = 0;

    if (you.duration[DUR_REPEL_UNDEAD] > 1)
        you.duration[DUR_REPEL_UNDEAD]--;

    if (you.duration[DUR_REPEL_UNDEAD] == 4)
    {
        mpr( "Your holy aura is starting to fade.", MSGCH_DURATION );
        you.duration[DUR_REPEL_UNDEAD] -= random2(3);
    }

    if (you.duration[DUR_REPEL_UNDEAD] == 1)
    {
        mpr( "Your holy aura fades away.", MSGCH_DURATION );
        you.duration[DUR_REPEL_UNDEAD] = 0;
    }

    // paradox: it both lasts longer & does more damage overall if you're
    //          moving slower.
    // rationalisation: I guess it gets rubbed off/falls off/etc if you
    //          move around more.
    if (you.duration[DUR_LIQUID_FLAMES] > 0)
        you.duration[DUR_LIQUID_FLAMES]--;

    if (you.duration[DUR_LIQUID_FLAMES] != 0)
    {
        const int res_fire = player_res_fire();

        mpr( "You are covered in liquid flames!", MSGCH_WARN );
        expose_player_to_element(BEAM_NAPALM, 12);

        if (res_fire > 0)
        {
            ouch( (((random2avg(9, 2) + 1) * you.time_taken) /
                    (1 + (res_fire * res_fire))) / 10, 0, KILLED_BY_BURNING );
        }

        if (res_fire <= 0)
        {
            ouch(((random2avg(9, 2) + 1) * you.time_taken) / 10, 0,
                 KILLED_BY_BURNING);
        }

        if (res_fire < 0)
        {
            ouch(((random2avg(9, 2) + 1) * you.time_taken) / 10, 0,
                 KILLED_BY_BURNING);
        }

        if (you.duration[DUR_CONDENSATION_SHIELD] > 0)
        {
            mpr("Your icy shield dissipates!", MSGCH_DURATION);
            you.duration[DUR_CONDENSATION_SHIELD] = 0;
            you.redraw_armour_class = 1;
        }
    }

    if (you.duration[DUR_ICY_ARMOUR] > 1)
    {
        you.duration[DUR_ICY_ARMOUR]--;
        //scrolls_burn(4, OBJ_POTIONS);
    }
    else if (you.duration[DUR_ICY_ARMOUR] == 1)
    {
        mpr("Your icy armour evaporates.", MSGCH_DURATION);
        you.redraw_armour_class = 1;     // is this needed? 2apr2000 {dlb}
        you.duration[DUR_ICY_ARMOUR] = 0;
    }

    if (you.duration[DUR_REPEL_MISSILES] > 1)
    {
        you.duration[DUR_REPEL_MISSILES]--;
        if (you.duration[DUR_REPEL_MISSILES] == 6)
        {
            mpr("Your repel missiles spell is about to expire...", MSGCH_DURATION);
            if (coinflip())
                you.duration[DUR_REPEL_MISSILES]--;
        }
    }
    else if (you.duration[DUR_REPEL_MISSILES] == 1)
    {
        mpr("You feel less protected from missiles.", MSGCH_DURATION);
        you.duration[DUR_REPEL_MISSILES] = 0;
    }

    if (you.duration[DUR_DEFLECT_MISSILES] > 1)
    {
        you.duration[DUR_DEFLECT_MISSILES]--;
        if (you.duration[DUR_DEFLECT_MISSILES] == 6)
        {
            mpr("Your deflect missiles spell is about to expire...", MSGCH_DURATION);
            if (coinflip())
                you.duration[DUR_DEFLECT_MISSILES]--;
        }
    }
    else if (you.duration[DUR_DEFLECT_MISSILES] == 1)
    {
        mpr("You feel less protected from missiles.", MSGCH_DURATION);
        you.duration[DUR_DEFLECT_MISSILES] = 0;
    }

    if (you.duration[DUR_REGENERATION] > 1)
    {
        you.duration[DUR_REGENERATION]--;

        if (you.duration[DUR_REGENERATION] == 6)
        {
            mpr("Your skin is crawling a little less now.", MSGCH_DURATION);
            if (coinflip())
                you.duration[DUR_REGENERATION]--;
        }
    }
    else if (you.duration[DUR_REGENERATION] == 1)
    {
        mpr("Your skin stops crawling.", MSGCH_DURATION);
        you.duration[DUR_REGENERATION] = 0;
    }

    if (you.duration[DUR_PRAYER] > 1)
        you.duration[DUR_PRAYER]--;
    else if (you.duration[DUR_PRAYER] == 1)
    {
        mpr( "Your prayer is over.", MSGCH_PRAY, you.religion );
        about_to_autopray = true;
        you.duration[DUR_PRAYER] = 0;
    }

    //jmf: more flexible weapon branding code
    if (you.duration[DUR_WEAPON_BRAND] > 1)
        you.duration[DUR_WEAPON_BRAND]--;
    else if (you.duration[DUR_WEAPON_BRAND] == 1)
    {
        const int wpn = you.equip[EQ_WEAPON];
        const int temp_effect = get_weapon_brand( you.inv[wpn] );

        you.duration[DUR_WEAPON_BRAND] = 0;

        char str_pass[ITEMNAME_SIZE];

        set_item_ego_type( you.inv[wpn], OBJ_WEAPONS, SPWPN_NORMAL );
        in_name(wpn, DESC_CAP_YOUR, str_pass);
        strncpy(info, str_pass, INFO_SIZE);

        switch (temp_effect)
        {
        case SPWPN_VORPAL:
            if (get_vorpal_type(you.inv[you.equip[EQ_WEAPON]])
                    == DVORP_SLICING)
                strcat(info, " seems blunter.");
            else
                strcat(info, " feels lighter.");
            break;

        case SPWPN_FLAMING:
            strcat(info, " goes out.");
            break;
        case SPWPN_FREEZING:
            strcat(info, " stops glowing.");
            break;
        case SPWPN_VENOM:
            strcat(info, " stops dripping with poison.");
            break;
        case SPWPN_DRAINING:
            strcat(info, " stops crackling.");
            break;
        case SPWPN_DISTORTION:
            strcat( info, " seems straighter." );
            // [dshaligram] Makes the brand unusable
            // miscast_effect( SPTYP_TRANSLOCATION, 9, 90, 100, "a distortion effect" );
            break;
        default:
            strcat(info, " seems inexplicably less special.");
            break;
        }

        //you.attribute[ATTR_WEAPON_BRAND] = 0;
        mpr(info, MSGCH_DURATION);
        you.wield_change = true;
    }

    if (you.duration[DUR_BREATH_WEAPON] > 1)
        you.duration[DUR_BREATH_WEAPON]--;
    else if (you.duration[DUR_BREATH_WEAPON] == 1)
    {
        mpr("You have got your breath back.", MSGCH_RECOVERY);
        you.duration[DUR_BREATH_WEAPON] = 0;
    }

    if (you.duration[DUR_TRANSFORMATION] > 1)
    {
        you.duration[DUR_TRANSFORMATION]--;

        if (you.duration[DUR_TRANSFORMATION] == 10)
        {
            mpr("Your transformation is almost over.", MSGCH_DURATION);
            you.duration[DUR_TRANSFORMATION] -= random2(3);
        }
    }
    else if (you.duration[DUR_TRANSFORMATION] == 1)
    {
        untransform();
        you.duration[DUR_BREATH_WEAPON] = 0;
    }

    if (you.duration[DUR_SWIFTNESS] > 1)
    {
        you.duration[DUR_SWIFTNESS]--;
        if (you.duration[DUR_SWIFTNESS] == 6)
        {
            mpr("You start to feel a little slower.", MSGCH_DURATION);
            if (coinflip())
                you.duration[DUR_SWIFTNESS]--;
        }
    }
    else if (you.duration[DUR_SWIFTNESS] == 1)
    {
        mpr("You feel sluggish.", MSGCH_DURATION);
        you.duration[DUR_SWIFTNESS] = 0;
    }

    if (you.duration[DUR_INSULATION] > 1)
    {
        you.duration[DUR_INSULATION]--;
        if (you.duration[DUR_INSULATION] == 6)
        {
            mpr("You start to feel a little less insulated.", MSGCH_DURATION);
            if (coinflip())
                you.duration[DUR_INSULATION]--;
        }
    }
    else if (you.duration[DUR_INSULATION] == 1)
    {
        mpr("You feel conductive.", MSGCH_DURATION);
        you.duration[DUR_INSULATION] = 0;
    }

    if (you.duration[DUR_STONEMAIL] > 1)
    {
        you.duration[DUR_STONEMAIL]--;
        if (you.duration[DUR_STONEMAIL] == 6)
        {
            mpr("Your scaly stone armour is starting to flake away.", MSGCH_DURATION);
            you.redraw_armour_class = 1;
            if (coinflip())
                you.duration[DUR_STONEMAIL]--;
        }
    }
    else if (you.duration[DUR_STONEMAIL] == 1)
    {
        mpr("Your scaly stone armour disappears.", MSGCH_DURATION);
        you.duration[DUR_STONEMAIL] = 0;
        you.redraw_armour_class = 1;
        burden_change();
    }

    if (you.duration[DUR_FORESCRY] > 1) //jmf: added
        you.duration[DUR_FORESCRY]--;
    else if (you.duration[DUR_FORESCRY] == 1)
    {
        mpr("You feel firmly rooted in the present.", MSGCH_DURATION);
        you.duration[DUR_FORESCRY] = 0;
        you.redraw_evasion = 1; 
    }

    if (you.duration[DUR_SEE_INVISIBLE] > 1)    //jmf: added
        you.duration[DUR_SEE_INVISIBLE]--;
    else if (you.duration[DUR_SEE_INVISIBLE] == 1)
    {
        you.duration[DUR_SEE_INVISIBLE] = 0;

        if (!player_see_invis())
            mpr("Your eyesight blurs momentarily.", MSGCH_DURATION);
    }

    if (you.duration[DUR_SILENCE] > 0)  //jmf: cute message handled elsewhere
        you.duration[DUR_SILENCE]--;

    if (you.duration[DUR_CONDENSATION_SHIELD] > 1)
    {
        you.duration[DUR_CONDENSATION_SHIELD]--;

        // [dshaligram] Makes this spell useless
        // scrolls_burn( 1, OBJ_POTIONS );
        
        if (player_res_cold() < 0)
        {
            mpr( "You feel very cold." );
            ouch( 2 + random2avg(13, 2), 0, KILLED_BY_FREEZING );
        }
    }
    else if (you.duration[DUR_CONDENSATION_SHIELD] == 1)
    {
        you.duration[DUR_CONDENSATION_SHIELD] = 0;
        mpr("Your icy shield evaporates.", MSGCH_DURATION);
        you.redraw_armour_class = 1;
    }

    if (you.duration[DUR_STONESKIN] > 1)
        you.duration[DUR_STONESKIN]--;
    else if (you.duration[DUR_STONESKIN] == 1)
    {
        mpr("Your skin feels tender.", MSGCH_DURATION);
        you.redraw_armour_class = 1;
        you.duration[DUR_STONESKIN] = 0;
    }

    if (you.duration[DUR_GLAMOUR] > 1)  //jmf: actually GLAMOUR_RELOAD, like
        you.duration[DUR_GLAMOUR]--;    //     the breath weapon delay
    else if (you.duration[DUR_GLAMOUR] == 1)
    {
        you.duration[DUR_GLAMOUR] = 0;
        //FIXME: cute message or not?
    }

    if (you.duration[DUR_TELEPORT] > 1)
        you.duration[DUR_TELEPORT]--;
    else if (you.duration[DUR_TELEPORT] == 1)
    {
        // only to a new area of the abyss sometimes (for abyss teleports)
        you_teleport2( true, one_chance_in(5) ); 
        you.duration[DUR_TELEPORT] = 0;
    }

    if (you.duration[DUR_CONTROL_TELEPORT] > 1)
    {
        you.duration[DUR_CONTROL_TELEPORT]--;

        if (you.duration[DUR_CONTROL_TELEPORT] == 6)
        {
            mpr("You start to feel a little uncertain.", MSGCH_DURATION);
            if (coinflip())
                you.duration[DUR_CONTROL_TELEPORT]--;
        }
    }
    else if (you.duration[DUR_CONTROL_TELEPORT] == 1)
    {
        mpr("You feel uncertain.", MSGCH_DURATION);
        you.duration[DUR_CONTROL_TELEPORT] = 0;
    }

    if (you.duration[DUR_RESIST_POISON] > 1)
    {
        you.duration[DUR_RESIST_POISON]--;
        if (you.duration[DUR_RESIST_POISON] == 6)
        {
            mpr("Your poison resistance is about to expire.", MSGCH_DURATION);
            if (coinflip())
                you.duration[DUR_RESIST_POISON]--;
        }
    }
    else if (you.duration[DUR_RESIST_POISON] == 1)
    {
        mpr("Your poison resistance expires.", MSGCH_DURATION);
        you.duration[DUR_RESIST_POISON] = 0;
    }

    if (you.duration[DUR_DEATH_CHANNEL] > 1)
    {
        you.duration[DUR_DEATH_CHANNEL]--;
        if (you.duration[DUR_DEATH_CHANNEL] == 6)
        {
            mpr("Your unholy channel is weakening.", MSGCH_DURATION);
            if (coinflip())
                you.duration[DUR_DEATH_CHANNEL]--;
        }
    }
    else if (you.duration[DUR_DEATH_CHANNEL] == 1)
    {
        mpr("Your unholy channel expires.", MSGCH_DURATION);    // Death channel wore off
        you.duration[DUR_DEATH_CHANNEL] = 0;
    }

    if (you.invis > 1)
    {
        you.invis--;

        if (you.invis == 6)
        {
            mpr("You flicker for a moment.", MSGCH_DURATION);
            if (coinflip())
                you.invis--;
        }
    }
    else if (you.invis == 1)
    {
        mpr("You flicker back into view.", MSGCH_DURATION);
        you.invis = 0;
    }

    if (you.conf > 0)
        reduce_confuse_player(1);

    if (you.paralysis > 1)
        you.paralysis--;
    else if (you.paralysis == 1)
    {
        mpr("You can move again.", MSGCH_DURATION);
        you.paralysis = 0;
    }

    if (you.exhausted > 1)
        you.exhausted--;
    else if (you.exhausted == 1)
    {
        mpr("You feel less fatigued.", MSGCH_DURATION);
        you.exhausted = 0;
    }

    dec_slow_player();
    dec_haste_player();

    if (you.might > 1)
        you.might--;
    else if (you.might == 1)
    {
        mpr("You feel a little less mighty now.", MSGCH_DURATION);
        you.might = 0;
        modify_stat(STAT_STRENGTH, -5, true);
    }

    if (you.berserker > 1)
        you.berserker--;
    else if (you.berserker == 1)
    {
        mpr( "You are no longer berserk.", MSGCH_DURATION );
        you.berserker = 0;

        //jmf: guilty for berserking /after/ berserk
        did_god_conduct( DID_STIMULANTS, 6 + random2(6) );

        //
        // Sometimes berserk leaves us physically drained
        //

        // chance of passing out:  
        //     - mutation gives a large plus in order to try and
        //       avoid the mutation being a "death sentence" to
        //       certain characters.
        //     - knowing the spell gives an advantage just
        //       so that people who have invested 3 spell levels
        //       are better off than the casual potion drinker...
        //       this should make it a bit more interesting for
        //       Crusaders again.
        //     - similarly for the amulet
        int chances[4];
        chances[0] = 10;
        chances[1] = you.mutation[MUT_BERSERK] * 25;
        chances[2] = (wearing_amulet( AMU_RAGE ) ? 10 : 0);
        chances[3] = (player_has_spell( SPELL_BERSERKER_RAGE ) ? 5 : 0);
        const char* reasons[4] = {
            "You struggle, and manage to stay standing.",
            "Your mutated body refuses to collapse.",
            "You feel your neck pulse as blood rushes through your body.",
            "Your mind masters your body."
        };
        const int chance = chances[0] + chances[1] + chances[2] + chances[3];

        if (you.berserk_penalty == NO_BERSERK_PENALTY)
            mpr("The very source of your rage keeps you on your feet.");
        // Note the beauty of Trog!  They get an extra save that's at
        // the very least 20% and goes up to 100%.
        else if ( you.religion == GOD_TROG && you.piety > random2(150) &&
                  !player_under_penance() )
            mpr("Trog's vigour flows through your veins.");
        else if ( !one_chance_in(chance) )
        {
            // Survived the probabilistic check.
            // Figure out why.

            int cause = random2(chance); // philosophically speaking...
            int i;
            for ( i = 0; i < 4; ++i )
            {
                if ( cause < chances[i] )
                {
                    // only print a reason if it actually exists
                    if ( reasons[i][0] != 0 )
                        mpr(reasons[i]);
                    break;
                }
                else
                    cause -= chances[i];
            }
            if (i == 4)
                mpr("Oops. Couldn't find a reason. Well, lucky you.");
        }
        else
        {
            mpr("You pass out from exhaustion.", MSGCH_WARN);
            you.paralysis += roll_dice( 1, 4 );
        }
        if ( you.paralysis == 0 )
            mpr("You are exhausted.", MSGCH_WARN);

        // this resets from an actual penalty or from NO_BERSERK_PENALTY
        you.berserk_penalty = 0;

        int dur = 12 + roll_dice( 2, 12 );
        you.exhausted += dur;
        slow_player( dur );

        make_hungry(700, true);

        if (you.hunger < 50)
            you.hunger = 50;

        calc_hp();
    }

    if (you.confusing_touch > 1)
        you.confusing_touch--;
    else if (you.confusing_touch == 1)
    {
        snprintf( info, INFO_SIZE, "Your %s stop glowing.", your_hand(true) );
        mpr( info, MSGCH_DURATION );
        you.confusing_touch = 0;
    }

    if (you.sure_blade > 1)
        you.sure_blade--;
    else if (you.sure_blade == 1)
    {
        mpr("The bond with your blade fades away.", MSGCH_DURATION);
        you.sure_blade = 0;
    }

    if (you.levitation > 1)
    {
        if (you.species != SP_KENKU || you.experience_level < 15)
            you.levitation--;

        if (player_equip_ego_type( EQ_BOOTS, SPARM_LEVITATION ))
            you.levitation = 2;

        if (you.levitation == 10)
        {
            mpr("You are starting to lose your buoyancy!", MSGCH_DURATION);
            you.levitation -= random2(6);

            if (you.duration[DUR_CONTROLLED_FLIGHT] > 0)
                you.duration[DUR_CONTROLLED_FLIGHT] = you.levitation;
        }
    }
    else if (you.levitation == 1)
    {
        mpr("You float gracefully downwards.", MSGCH_DURATION);
        you.levitation = 0;
        burden_change();
        you.duration[DUR_CONTROLLED_FLIGHT] = 0;

        // re-enter the terrain:
        move_player_to_grid( you.x_pos, you.y_pos, false, true, true );
    }

    if (you.rotting > 0)
    {
        // XXX: Mummies have an ability (albeit an expensive one) that 
        // can fix rotted HPs now... it's probably impossible for them
        // to even start rotting right now, but that could be changed. -- bwr
        if (you.species == SP_MUMMY)
            you.rotting = 0;
        else if (random2(20) <= (you.rotting - 1))
        {
            mpr("You feel your flesh rotting away.", MSGCH_WARN);
            ouch(1, 0, KILLED_BY_ROTTING);
            rot_hp(1);
            you.rotting--;
        }
    }

    // ghoul rotting is special, but will deduct from you.rotting
    // if it happens to be positive - because this is placed after
    // the "normal" rotting check, rotting attacks can be somewhat
    // more painful on ghouls - reversing order would make rotting
    // attacks somewhat less painful, but that seems wrong-headed {dlb}:
    if (you.species == SP_GHOUL)
    {
        if (one_chance_in(400))
        {
            mpr("You feel your flesh rotting away.", MSGCH_WARN);
            ouch(1, 0, KILLED_BY_ROTTING);
            rot_hp(1);

            if (you.rotting > 0)
                you.rotting--;
        }
    }

    dec_disease_player();

    if (you.poison > 0)
    {
        if (random2(5) <= (you.poison - 1))
        {
            if (you.poison > 10 && random2(you.poison) >= 8)
            {
                ouch(random2(10) + 5, 0, KILLED_BY_POISON);
                mpr("You feel extremely sick.", MSGCH_DANGER);
            }
            else if (you.poison > 5 && coinflip())
            {
                ouch((coinflip()? 3 : 2), 0, KILLED_BY_POISON);
                mpr("You feel very sick.", MSGCH_WARN);
            }
            else
            {
                // the poison running through your veins.");
                ouch(1, 0, KILLED_BY_POISON);
                mpr("You feel sick.");
            }

            if ((you.hp == 1 && one_chance_in(3)) || one_chance_in(8))
                reduce_poison_player(1);
        }
    }

    if (you.deaths_door)
    {
        if (you.hp > allowed_deaths_door_hp())
        {
            mpr("Your life is in your own hands once again.", MSGCH_DURATION);
            you.paralysis += 5 + random2(5);
            confuse_player( 10 + random2(10) );
            you.hp_max--;
            deflate_hp(you.hp_max, false);
            you.deaths_door = 0;
        }
        else
            you.deaths_door--;

        if (you.deaths_door == 10)
        {
            mpr("Your time is quickly running out!", MSGCH_DURATION);
            you.deaths_door -= random2(6);
        }
        if (you.deaths_door == 1)
        {
            mpr("Your life is in your own hands again!", MSGCH_DURATION);
            more();
        }
    }
}

static void check_banished()
{
    if (you.banished)
    {
        you.banished = false;

        if (you.level_type != LEVEL_ABYSS)
        {
            mpr("You are cast into the Abyss!");
            banished(DNGN_ENTER_ABYSS);
        }
    }
}

/* Perhaps we should write functions like: update_repel_undead(),
   update_liquid_flames(), and so on. Even better, we could have a
   vector of callback functions (or objects) which get installed
   at some point.
*/

static void world_reacts()
{

    bool its_quiet;             //jmf: for silence messages

    if (you.num_turns != -1)
    {
        you.num_turns++;
        update_turn_count();
    }
    check_banished();

    run_environment_effects();

    if ( !you.paralysis && !you.mutation[MUT_BLURRY_VISION] &&
         (you.mutation[MUT_ACUTE_VISION] >= 2 ||
          random2(30) < you.skills[SK_TRAPS_DOORS]) )
        search_around(true); // only check adjacent squares

    stealth = check_stealth();

#if 0
    // too annoying for regular diagnostics
    snprintf( info, INFO_SIZE, "stealth: %d", stealth );
    mpr( info, MSGCH_DIAGNOSTICS );
#endif

    if (you.special_wield != SPWLD_NONE)
        special_wielded();

    if (one_chance_in(10))
    {   
        // this is instantaneous
        if (player_teleport() > 0 && one_chance_in(100 / player_teleport()))
            you_teleport2( true ); 
        else if (you.level_type == LEVEL_ABYSS && one_chance_in(30))
            you_teleport2( false, true ); // to new area of the Abyss
    }

    if (env.cgrid[you.x_pos][you.y_pos] != EMPTY_CLOUD)
        in_a_cloud();

    decrement_durations();

    const int food_use = player_hunger_rate();

    if (food_use > 0 && you.hunger >= 40)
        make_hungry( food_use, true );

    // XXX: using an int tmp to fix the fact that hit_points_regeneration
    // is only an unsigned char and is thus likely to overflow. -- bwr
    int tmp = static_cast< int >( you.hit_points_regeneration );

    if (you.hp < you.hp_max && !you.disease && !you.deaths_door)
        tmp += player_regen();

    while (tmp >= 100)
    {
        inc_hp(1, false);
        tmp -= 100;

        you.running.check_hp();
    }

    ASSERT( tmp >= 0 && tmp < 100 );
    you.hit_points_regeneration = static_cast< unsigned char >( tmp );

    // XXX: Doing the same as the above, although overflow isn't an
    // issue with magic point regeneration, yet. -- bwr
    tmp = static_cast< int >( you.magic_points_regeneration );

    if (you.magic_points < you.max_magic_points)
        tmp += 7 + you.max_magic_points / 2;

    while (tmp >= 100)
    {
        inc_mp(1, false);
        tmp -= 100;

        you.running.check_mp();
    }

    ASSERT( tmp >= 0 && tmp < 100 );
    you.magic_points_regeneration = static_cast< unsigned char >( tmp );

    // If you're wielding a rod, it'll gradually recharge.
    recharge_rods();

    viewwindow(1, true);

    handle_monsters();
    check_banished();

    ASSERT(you.time_taken >= 0);
    // make sure we don't overflow
    ASSERT(DBL_MAX - you.elapsed_time > you.time_taken);

    you.elapsed_time += you.time_taken;

    if (you.synch_time <= you.time_taken)
    {
        handle_time(200 + (you.time_taken - you.synch_time));
        you.synch_time = 200;
    }
    else
    {
        you.synch_time -= you.time_taken;
    }

    manage_clouds();

    if (you.fire_shield > 0)
        manage_fire_shield();

    // food death check:
    if (you.is_undead != US_UNDEAD && you.hunger <= 500)
    {
        if (!you.paralysis && one_chance_in(40))
        {
            mpr("You lose consciousness!", MSGCH_FOOD);
            you.paralysis += 5 + random2(8);

            if (you.paralysis > 13)
                you.paralysis = 13;
        }

        if (you.hunger <= 100)
        {
            mpr( "You have starved to death.", MSGCH_FOOD );
            ouch( INSTANT_DEATH, 0, KILLED_BY_STARVATION );
        }
    }

    //jmf: added silence messages
    its_quiet = silenced(you.x_pos, you.y_pos);

    if (you.attribute[ATTR_WAS_SILENCED] != its_quiet)
    {
        if (its_quiet)
        {
            if (random2(30))
                mpr("You are enveloped in profound silence.", MSGCH_WARN);
            else
                mpr("The dungeon seems quiet ... too quiet!", MSGCH_WARN);
        }
        else
        {
            mpr("Your hearing returns.", MSGCH_RECOVERY);
        }

        you.attribute[ATTR_WAS_SILENCED] = its_quiet;
    }

    viewwindow(1, false);

    if (you.paralysis > 0 && any_messages())
        more();

    // place normal dungeon monsters,  but not in player LOS
    if (you.level_type == LEVEL_DUNGEON
        && !player_in_branch( BRANCH_ECUMENICAL_TEMPLE )
        && one_chance_in((you.char_direction == DIR_DESCENDING) ? 240 : 10))
    {
        int prox = (one_chance_in(10) ? PROX_NEAR_STAIRS 
                                      : PROX_AWAY_FROM_PLAYER);

        // The rules change once the player has picked up the Orb...
        if (you.char_direction == DIR_ASCENDING)
            prox = (one_chance_in(10) ? PROX_CLOSE_TO_PLAYER : PROX_ANYWHERE);

        mons_place( WANDERING_MONSTER, BEH_HOSTILE, MHITNOT, false,
                    50, 50, LEVEL_DUNGEON, prox );
    }

    // place Abyss monsters.
    if (you.level_type == LEVEL_ABYSS && one_chance_in(5))
    {
        mons_place( WANDERING_MONSTER, BEH_HOSTILE, MHITNOT, false,
                    50, 50, LEVEL_ABYSS, PROX_ANYWHERE );
    }

    // place Pandemonium monsters
    if (you.level_type == LEVEL_PANDEMONIUM && one_chance_in(50))
        pandemonium_mons();

    // No monsters in the Labyrinth, or the Ecumenical Temple
    return;
}

static command_type get_next_cmd()
{
    if (Options.autoprayer_on && you.duration[DUR_PRAYER] == 0 &&
        just_autoprayed == false && you.religion != GOD_NO_GOD &&
        grid_altar_god( grd[you.x_pos][you.y_pos] ) == GOD_NO_GOD &&
        i_feel_safe())
    {
        just_autoprayed = true;
        about_to_autopray = false;
        return CMD_PRAY;
    }
    if ( just_autoprayed && you.duration[DUR_PRAYER] == 0 )
    {
        /* oops */
        mpr("Autoprayer failed, deactivating.", MSGCH_WARN);
        Options.autoprayer_on = false;
    }
    just_autoprayed = false;
    if ( Options.autoprayer_on && about_to_autopray &&
         you.religion != GOD_NO_GOD &&
         you.duration[DUR_PRAYER] == 0 )
    {
        mpr("Autoprayer not resuming prayer.", MSGCH_WARN);
        about_to_autopray = false;
    }

#if DEBUG_DIAGNOSTICS
    // save hunger at start of round
    // for use with hunger "delta-meter" in  output.cc
    you.old_hunger = you.hunger;        
#endif
    
#if DEBUG_ITEM_SCAN
    debug_item_scan();
#endif
    keycode_type keyin = get_next_keycode();

    if (is_userfunction(keyin))
    {
        run_macro(get_userfunction(keyin));
        return (CMD_NEXT_CMD);
    }

    return keycode_to_command(keyin);
}

/* for now, this is an extremely yucky hack */
command_type keycode_to_command( keycode_type key ) {
    switch ( key ) {
    case 'b': return CMD_MOVE_DOWN_LEFT;
    case 'h': return CMD_MOVE_LEFT;
    case 'j': return CMD_MOVE_DOWN;
    case 'k': return CMD_MOVE_UP;
    case 'l': return CMD_MOVE_RIGHT;
    case 'n': return CMD_MOVE_DOWN_RIGHT;
    case 'u': return CMD_MOVE_UP_RIGHT;
    case 'y': return CMD_MOVE_UP_LEFT;

    case 'a': return CMD_USE_ABILITY;
    case 'c': return CMD_CLOSE_DOOR;
    case 'd': return CMD_DROP;
    case 'e': return CMD_EAT;
    case 'f': return CMD_FIRE;
    case 'g': return CMD_PICKUP;
    case 'i': return CMD_DISPLAY_INVENTORY;
    case 'm': return CMD_DISPLAY_SKILLS;
    case 'o': return CMD_OPEN_DOOR;
    case 'p': return CMD_PRAY;
    case 'q': return CMD_QUAFF;
    case 'r': return CMD_READ;
    case 's': return CMD_SEARCH;
    case 't': return CMD_THROW;
    case 'v': return CMD_EXAMINE_OBJECT;
    case 'w': return CMD_WIELD_WEAPON;
    case 'x': return CMD_LOOK_AROUND;
    case 'z': return CMD_ZAP_WAND;

    case 'B': return CMD_RUN_DOWN_LEFT;
    case 'H': return CMD_RUN_LEFT;
    case 'J': return CMD_RUN_DOWN;
    case 'K': return CMD_RUN_UP;
    case 'L': return CMD_RUN_RIGHT;
    case 'N': return CMD_RUN_DOWN_RIGHT;
    case 'U': return CMD_RUN_UP_RIGHT;
    case 'Y': return CMD_RUN_UP_LEFT;

    case 'A': return CMD_DISPLAY_MUTATIONS;
    case 'C': return CMD_EXPERIENCE_CHECK;
    case 'D': return CMD_BUTCHER;
    case 'E': return CMD_EVOKE;
    case 'F': return CMD_NO_CMD;
    case 'G': return CMD_NO_CMD;
    case 'I': return CMD_NO_CMD;
    case 'M': return CMD_MEMORISE_SPELL;
    case 'O': return CMD_DISPLAY_OVERMAP;
    case 'P': return CMD_WEAR_JEWELLERY;
    case 'Q': return CMD_QUIT;
    case 'R': return CMD_REMOVE_JEWELLERY;
    case 'S': return CMD_SAVE_GAME;
    case 'T': return CMD_REMOVE_ARMOUR;
    case 'V': return CMD_GET_VERSION;
    case 'W': return CMD_WEAR_ARMOUR;
    case 'X': return CMD_DISPLAY_MAP;
    case 'Z': return CMD_CAST_SPELL;

    case '.': return CMD_MOVE_NOWHERE;
    case '<': return CMD_GO_UPSTAIRS;
    case '>': return CMD_GO_DOWNSTAIRS;
    case '@': return CMD_DISPLAY_CHARACTER_STATUS;
    case '%': return CMD_RESISTS_SCREEN;
    case ',': return CMD_PICKUP;
    case ':': return CMD_MAKE_NOTE;
    case '_': return CMD_DISPLAY_NOTES;
    case ';': return CMD_INSPECT_FLOOR;
    case '!': return CMD_SHOUT;
    case '^': return CMD_DISPLAY_RELIGION;
    case '#': return CMD_CHARACTER_DUMP;
    case '=': return CMD_ADJUST_INVENTORY;
    case '?': return CMD_DISPLAY_COMMANDS;
    case '~': return CMD_MACRO_ADD;
    case '&': return CMD_WIZARD;
    case '"': return CMD_LIST_JEWELLERY;
    case '{': return CMD_INSCRIBE_ITEM;
    case '[': return CMD_LIST_ARMOUR;
    case ']': return CMD_LIST_ARMOUR;
    case ')': return CMD_LIST_WEAPONS;
    case '(': return CMD_LIST_WEAPONS;
    case '\\': return CMD_DISPLAY_KNOWN_OBJECTS;
    case '\'': return CMD_WEAPON_SWAP;

    case '0': return CMD_NO_CMD;
    case '5': return CMD_REST;

    case CONTROL('B'): return CMD_OPEN_DOOR_DOWN_LEFT;
    case CONTROL('H'): return CMD_OPEN_DOOR_LEFT;
    case CONTROL('J'): return CMD_OPEN_DOOR_DOWN;
    case CONTROL('K'): return CMD_OPEN_DOOR_UP;
    case CONTROL('L'): return CMD_OPEN_DOOR_RIGHT;
    case CONTROL('N'): return CMD_OPEN_DOOR_DOWN_RIGHT;
    case CONTROL('U'): return CMD_OPEN_DOOR_UP_LEFT;
    case CONTROL('Y'): return CMD_OPEN_DOOR_UP_RIGHT;

    case CONTROL('A'): return CMD_TOGGLE_AUTOPICKUP;
    case CONTROL('C'): return CMD_CLEAR_MAP;
    case CONTROL('D'): return CMD_NO_CMD;
    case CONTROL('E'): return CMD_FORGET_STASH;
    case CONTROL('F'): return CMD_SEARCH_STASHES;
    case CONTROL('G'): return CMD_INTERLEVEL_TRAVEL;
    case CONTROL('I'): return CMD_NO_CMD;
    case CONTROL('M'): return CMD_NO_CMD;
    case CONTROL('O'): return CMD_EXPLORE;
    case CONTROL('P'): return CMD_REPLAY_MESSAGES;
    case CONTROL('Q'): return CMD_NO_CMD;
    case CONTROL('R'): return CMD_REDRAW_SCREEN;
    case CONTROL('S'): return CMD_MARK_STASH;
    case CONTROL('T'): return CMD_TOGGLE_NOFIZZLE;
    case CONTROL('V'): return CMD_TOGGLE_AUTOPRAYER;
    case CONTROL('W'): return CMD_FIX_WAYPOINT;
    case CONTROL('X'): return CMD_SAVE_GAME_NOW;
    case CONTROL('Z'): return CMD_SUSPEND_GAME;
    default: return CMD_NO_CMD;
    }
}

keycode_type get_next_keycode()
{
    keycode_type keyin;

    flush_input_buffer( FLUSH_BEFORE_COMMAND );
    keyin = unmangle_direction_keys(getch_with_command_macros());
    mesclr();

    return (keyin);
}

static void middle_input() {
    if (Options.stash_tracking)
        stashes.update_visible_stashes(
            Options.stash_tracking == STM_ALL? 
            StashTracker::ST_AGGRESSIVE :
            StashTracker::ST_PASSIVE);
}

/*
   Opens doors and handles some aspects of untrapping. If either move_x or
   move_y are non-zero,  the pair carries a specific direction for the door
   to be opened (eg if you type ctrl - dir).
 */
static void open_door(int move_x, int move_y, bool check_confused)
{
    struct dist door_move;
    int dx, dy;             // door x, door y

    if (check_confused && you.conf && !one_chance_in(3))
    {
        move_x = random2(3) - 1;
        move_y = random2(3) - 1;
    }

    door_move.dx = move_x;
    door_move.dy = move_y;

    if (move_x || move_y)
    {
        // convenience
        dx = you.x_pos + move_x;
        dy = you.y_pos + move_y;

        const int mon = mgrd[dx][dy];

        if (mon != NON_MONSTER && !mons_is_submerged(&menv[mon]))
        {
            you_attack(mgrd[dx][dy], true);
            you.turn_is_over = true;

            if (you.berserk_penalty != NO_BERSERK_PENALTY)
                you.berserk_penalty = 0;

            return;
        }

        if (grd[dx][dy] >= DNGN_TRAP_MECHANICAL && grd[dx][dy] <= DNGN_TRAP_III)
        {
            if (env.cgrid[dx][dy] != EMPTY_CLOUD)
            {
                mpr("You can't get to that trap right now.");
                return;
            }

            disarm_trap(door_move);
            return;
        }

    } 
    else 
    {
        mpr("Which direction?", MSGCH_PROMPT);
        direction( door_move, DIR_DIR );
        if (!door_move.isValid)
            return;

        // convenience
        dx = you.x_pos + door_move.dx;
        dy = you.y_pos + door_move.dy;
    }

    if (grd[dx][dy] == DNGN_CLOSED_DOOR)
    {
        int skill = you.dex + (you.skills[SK_TRAPS_DOORS] + you.skills[SK_STEALTH]) / 2;

        if (one_chance_in(skill) && !silenced(you.x_pos, you.y_pos))
        {
            mpr( "As you open the door, it creaks loudly!" );
            noisy( 10, you.x_pos, you.y_pos );
        }
        else
        {
            mpr( player_is_levitating() ? "You reach down and open the door."
                                        : "You open the door." );
        }

        grd[dx][dy] = DNGN_OPEN_DOOR;
        you.turn_is_over = true;
    }
    else
    {
        mpr("You swing at nothing.");
        make_hungry(3, true);
        you.turn_is_over = true;
    }
}                               // end open_door()

/*
   Similar to open_door. Can you spot the difference?
 */
static void close_door(int door_x, int door_y)
{
    struct dist door_move;
    int dx, dy;             // door x, door y

    door_move.dx = door_x;
    door_move.dy = door_y;

    if (!(door_x || door_y))
    {
        mpr("Which direction?", MSGCH_PROMPT);
        direction( door_move, DIR_DIR );
        if (!door_move.isValid)
            return;
    }

    if (door_move.dx == 0 && door_move.dy == 0)
    {
        mpr("You can't close doors on yourself!");
        return;
    }

    // convenience
    dx = you.x_pos + door_move.dx;
    dy = you.y_pos + door_move.dy;

    if (grd[dx][dy] == DNGN_OPEN_DOOR)
    {
        if (mgrd[dx][dy] != NON_MONSTER)
        {
            // Need to make sure that turn_is_over is set if creature is 
            // invisible
            mpr("There's a creature in the doorway!");
            door_move.dx = 0;
            door_move.dy = 0;
            return;
        }

        if (igrd[dx][dy] != NON_ITEM)
        {
            mpr("There's something blocking the doorway.");
            door_move.dx = 0;
            door_move.dy = 0;
            return;
        }

        int skill = you.dex + (you.skills[SK_TRAPS_DOORS] + you.skills[SK_STEALTH]) / 2;

        if (one_chance_in(skill) && !silenced(you.x_pos, you.y_pos))
        {
            mpr("As you close the door, it creaks loudly!");
            noisy( 10, you.x_pos, you.y_pos );
        }
        else
        {
            mpr( player_is_levitating() ? "You reach down and close the door."
                                        : "You close the door." );
        }

        grd[dx][dy] = DNGN_CLOSED_DOOR;
        you.turn_is_over = true;
    }
    else
    {
        mpr("There isn't anything that you can close there!");
    }
}                               // end open_door()


// initialise whole lot of stuff...
// returns true if a new character
static bool initialise(void)
{
    bool ret;

    int i = 0, j = 0;           // counter variables {dlb}

    you.symbol = '@';
    you.colour = LIGHTGREY;

    seed_rng();
    clear_ids();                // in itemname.cc
    init_char_table(Options.char_set);
    init_feature_table();

    init_properties();
    init_monsters(mcolour);     // this needs to be way up top {dlb}
    init_playerspells();        // this needs to be way up top {dlb}

    // init item array:
    for (i = 0; i < MAX_ITEMS; i++)
        init_item( i );

    // empty messaging string
    strcpy(info, "");

    for (i = 0; i < MAX_MONSTERS; i++)
    {
        menv[i].type = -1;
        menv[i].speed_increment = 10;
        menv[i].flags = 0;
        menv[i].behaviour = BEH_SLEEP;

        menv[i].foe = NON_MONSTER;
        menv[i].attitude = ATT_HOSTILE;

        for (j = 0; j < NUM_MON_ENCHANTS; j++)
            menv[i].enchantment[j] = ENCH_NONE;

        for (j = 0; j < NUM_MONSTER_SLOTS; j++)
            menv[i].inv[j] = NON_ITEM;

        menv[i].number = 0;
    }

    for (i = 0; i < GXM; i++)
    {
        for (j = 0; j < GYM; j++)
        {
            igrd[i][j] = NON_ITEM;
            mgrd[i][j] = NON_MONSTER;
            env.map[i][j] = 0;
            env.map_col[i][j].clear();
        }
    }

    for (i = 0; i < 50; i++)
    {
        you.unique_creatures[i] = 0;
        you.unique_items[i] = UNIQ_NOT_EXISTS;
    }

    for (i = 0; i < NUM_STATUE_TYPES; i++)
        you.visible_statue[i] = 0;

    // initialize tag system before we try loading anything!
    tag_init();

    // Read special levels and vaults.
    read_maps();
    
    init_io();

#ifdef USE_MACROS
    // Load macros
    macro_init();
#endif

    // system initialisation stuff:
    textbackground(0);

#ifdef DOS
    directvideo = 1;
#endif

    clrscr();

    // sets up a new game:
    bool newc = new_game();
    ret = newc;  // newc will be mangled later so we'll take a copy --bwr

    if (!newc)
        restore_game();

    game_has_started = true;

    calc_hp();
    calc_mp();

    load( 82, (newc ? LOAD_START_GAME : LOAD_RESTART_GAME), false, 0, 
          you.where_are_you );

#if DEBUG_DIAGNOSTICS
    // Debug compiles display a lot of "hidden" information, so we auto-wiz
    you.wizard = true;
#endif

    init_properties();
    burden_change();
    make_hungry(0,true);

    you.redraw_strength = 1;
    you.redraw_intelligence = 1;
    you.redraw_dexterity = 1;
    you.redraw_armour_class = 1;
    you.redraw_evasion = 1;
    you.redraw_experience = 1;
    you.redraw_gold = 1;
    you.wield_change = true;

    you.start_time = time( NULL );      // start timer on session

    draw_border();
    new_level();
    update_turn_count();

    init_new_level(false);
    // Mark items in inventory as of unknown origin.
    origin_set_inventory(origin_set_unknown);

    // set vision radius to player's current vision
    setLOSRadius( you.current_vision );

    if (newc)
    {
        // For a new game, wipe out monsters in LOS.
        zap_los_monsters();
    }

#ifdef CLUA_BINDINGS
    clua.runhook("chk_startgame", "%b", ret);
    std::string yname = you.your_name;
    read_init_file(true);
    strncpy(you.your_name, yname.c_str(), kNameLen);
    you.your_name[kNameLen - 1] = 0;

    // In case Lua changed the character set.
    init_char_table(Options.char_set);
    init_feature_table();
#endif

    set_cursor_enabled(false);
    viewwindow(1, false);   // This just puts the view up for the first turn.

    activate_notes(true);

    crawl_state.need_save = true;

    return (ret);
}

// An attempt to tone down berserk a little bit. -- bwross
//
// This function does the accounting for not attacking while berserk
// This gives a triangular number function for the additional penalty
// Turn:    1  2  3   4   5   6   7   8
// Penalty: 1  3  6  10  15  21  28  36
//
// Total penalty (including the standard one during upkeep is:
//          2  5  9  14  20  27  35  44
//
static void do_berserk_no_combat_penalty(void)
{
    // Butchering/eating a corpse will maintain a blood rage.
    const int delay = current_delay_action();
    if (delay == DELAY_BUTCHER || delay == DELAY_EAT)
        return;

    if (you.berserk_penalty == NO_BERSERK_PENALTY)
        return;

    if (you.berserker)
    {
        you.berserk_penalty++;

        switch (you.berserk_penalty)
        {
        case 2:
            mpr("You feel a strong urge to attack something.", MSGCH_DURATION);
            break;
        case 4:
            mpr("You feel your anger subside.", MSGCH_DURATION);
            break;
        case 6:
            mpr("Your blood rage is quickly leaving you.", MSGCH_DURATION);
            break;
        }

        // I do these three separately, because the might and
        // haste counters can be different.
        you.berserker -= you.berserk_penalty;
        if (you.berserker < 1)
            you.berserker = 1;

        you.might -= you.berserk_penalty;
        if (you.might < 1)
            you.might = 1;

        you.haste -= you.berserk_penalty;
        if (you.haste < 1)
            you.haste = 1;
    }
    return;
}                               // end do_berserk_no_combat_penalty()


// Called when the player moves by walking/running. Also calls
// attack function and trap function etc when necessary.
static void move_player(int move_x, int move_y)
{
    bool attacking = false;
    bool moving = true;         // used to prevent eventual movement (swap)
    bool swap = false;

    if (you.conf)
    {
        if (!one_chance_in(3))
        {
            move_x = random2(3) - 1;
            move_y = random2(3) - 1;
        }

        const int new_targ_x = you.x_pos + move_x;
        const int new_targ_y = you.y_pos + move_y;
        if (!in_bounds(new_targ_x, new_targ_y)
                || grid_is_solid(grd[new_targ_x][new_targ_y]))
        {
            you.turn_is_over = true;
            mpr("Ouch!");
            apply_berserk_penalty = true;
            return;
        }
    } // end of if you.conf

    if (you.running.check_stop_running())
    {
        move_x = 0;
        move_y = 0;
        // [ds] Do we need this? Shouldn't it be false to start with?
        you.turn_is_over = false;
        return;
    }

    const int targ_x = you.x_pos + move_x;
    const int targ_y = you.y_pos + move_y;
    const unsigned char targ_grid  =  grd[ targ_x ][ targ_y ];
    const unsigned char targ_monst = mgrd[ targ_x ][ targ_y ];
    const bool          targ_solid = grid_is_solid(targ_grid);

    if (targ_monst != NON_MONSTER && !mons_is_submerged(&menv[targ_monst]))
    {
        struct monsters *mon = &menv[targ_monst];

        // you can swap places with a friendly monster if you
        // can see it and you're not confused
        if (mons_friendly( mon ) && player_monster_visible( mon ) && !you.conf)
        {
            if (swap_places( mon ))
                swap = true;
            else
                moving = false;
        }
        else // attack!
        {
            you_attack( targ_monst, true );
            you.turn_is_over = true;

            // we don't want to create a penalty if there isn't
            // supposed to be one
            if (you.berserk_penalty != NO_BERSERK_PENALTY)
                you.berserk_penalty = 0;

            attacking = true;
        }
    }

    if (!attacking && !targ_solid && moving)
    {
        you.time_taken *= player_movement_speed();
        you.time_taken /= 10;
        move_player_to_grid(targ_x, targ_y, true, false, swap);

        move_x = 0;
        move_y = 0;

        you.turn_is_over = true;
        item_check( false );
    }

    // BCR - Easy doors single move
    if (targ_grid == DNGN_CLOSED_DOOR && Options.easy_open)
        open_door(move_x, move_y, false);
    else if (targ_solid)
    {
        stop_running();
        move_x = 0;
        move_y = 0;
        you.turn_is_over = 0;
    }

    if (you.running == RMODE_START)
        you.running = RMODE_CONTINUE;

    if (you.level_type == LEVEL_ABYSS
            && (you.x_pos <= 15 || you.x_pos >= (GXM - 16)
                    || you.y_pos <= 15 || you.y_pos >= (GYM - 16)))
    {
        area_shift();
        you.pet_target = MHITNOT;

#if DEBUG_DIAGNOSTICS
        mpr( "Shifting.", MSGCH_DIAGNOSTICS );
        int igly = 0;
        int ig2 = 0;

        for (igly = 0; igly < MAX_ITEMS; igly++)
        {
            if (is_valid_item( mitm[igly] ))
                ig2++;
        }

        snprintf( info, INFO_SIZE, "Number of items present: %d", ig2 );
        mpr( info, MSGCH_DIAGNOSTICS );

        ig2 = 0;
        for (igly = 0; igly < MAX_MONSTERS; igly++)
        {
            if (menv[igly].type != -1)
                ig2++;
        }

        snprintf( info, INFO_SIZE, "Number of monsters present: %d", ig2 );
        mpr( info, MSGCH_DIAGNOSTICS );

        snprintf( info, INFO_SIZE, "Number of clouds present: %d", env.cloud_no );
        mpr( info, MSGCH_DIAGNOSTICS );
#endif
    }

    apply_berserk_penalty = !attacking;
}                               // end move_player()
