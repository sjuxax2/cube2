// creation of scoreboard
#include "cube.h"
#include "game.h"

namespace game
{
    VARP(scoreboard2d, 0, 1, 1);
    VARP(showclientnum, 0, 0, 1);
    VARP(showpj, 0, 0, 1);
    VARP(showping, 0, 1, 1);
    VARP(showspectators, 0, 1, 1);
    VARP(highlightscore, 0, 1, 1);
    VARP(showconnecting, 0, 0, 1);

    static int teamscorecmp(const teamscore *x, const teamscore *y)
    {
        if(x->score > y->score) return -1;
        if(x->score < y->score) return 1;
        return strcmp(x->team, y->team);
    }

    static int playersort(const fpsent **a, const fpsent **b)
    {
        if((*a)->state==CS_SPECTATOR)
        {
            if((*b)->state==CS_SPECTATOR) return strcmp((*a)->name, (*b)->name);
            else return 1;
        }
        else if((*b)->state==CS_SPECTATOR) return -1;
        if((*a)->frags > (*b)->frags) return -1;
        if((*a)->frags < (*b)->frags) return 1;
        return strcmp((*a)->name, (*b)->name);
    }

    void getbestplayers(vector<fpsent *> &best)
    {
        loopi(numdynents())
        {
            fpsent *o = (fpsent *)iterdynents(i);
            if(o && o->type==ENT_PLAYER && o->state!=CS_SPECTATOR) best.add(o);
        }
        best.sort(playersort);
        while(best.length()>1 && best.last()->frags < best[0]->frags) best.drop();
    }

    void sortteams(vector<teamscore> &teamscores)
    {
        if(cmode && cmode->hidefrags()) cmode->getteamscores(teamscores);

        loopi(numdynents())
        {
            fpsent *o = (fpsent *)iterdynents(i);
            if(o && o->type==ENT_PLAYER)
            {
                teamscore *ts = NULL;
                loopv(teamscores) if(!strcmp(teamscores[i].team, o->team)) { ts = &teamscores[i]; break; }
                if(!ts) teamscores.add(teamscore(o->team, cmode && cmode->hidefrags() ? 0 : o->frags));
                else if(!cmode || !cmode->hidefrags()) ts->score += o->frags;
            }
        }
        teamscores.sort(teamscorecmp);
    }

    void getbestteams(vector<const char *> &best)
    {
        vector<teamscore> teamscores;
        sortteams(teamscores);
        while(teamscores.length()>1 && teamscores.last().score < teamscores[0].score) teamscores.drop();
        loopv(teamscores) best.add(teamscores[i].team);
    }

    struct scoregroup : teamscore
    {
        vector<fpsent *> players;
    };
    static vector<scoregroup *> groups;
    static vector<fpsent *> spectators;

    static int scoregroupcmp(const scoregroup **x, const scoregroup **y)
    {
        if(!(*x)->team)
        {
            if((*y)->team) return 1;
        }
        else if(!(*y)->team) return -1;
        if((*x)->score > (*y)->score) return -1;
        if((*x)->score < (*y)->score) return 1;
        if((*x)->players.length() > (*y)->players.length()) return -1;
        if((*x)->players.length() < (*y)->players.length()) return 1;
        return (*x)->team && (*y)->team ? strcmp((*x)->team, (*y)->team) : 0;
    }

    static int groupplayers()
    {
        int numgroups = 0;
        spectators.setsize(0);
        loopi(numdynents())
        {
            fpsent *o = (fpsent *)iterdynents(i);
            if(!o || o->type!=ENT_PLAYER || (!showconnecting && !o->name[0])) continue;
            if(o->state==CS_SPECTATOR) { spectators.add(o); continue; }
            const char *team = m_teammode && o->team[0] ? o->team : NULL;
            bool found = false;
            loopj(numgroups)
            {
                scoregroup &g = *groups[j];
                if(team!=g.team && (!team || !g.team || strcmp(team, g.team))) continue;
                if(team && (!cmode || !cmode->hidefrags())) g.score += o->frags;
                g.players.add(o);
                found = true;
            }
            if(found) continue;
            if(numgroups>=groups.length()) groups.add(new scoregroup);
            scoregroup &g = *groups[numgroups++];
            g.team = team;
            if(!team) g.score = 0;
            else if(cmode && cmode->hidefrags()) g.score = cmode->getteamscore(o->team);
            else g.score = o->frags;
            g.players.setsize(0);
            g.players.add(o);
        }
        loopi(numgroups) groups[i]->players.sort(playersort);
        spectators.sort(playersort);
        groups.sort(scoregroupcmp, 0, numgroups);
        return numgroups;
    }

    void renderscoreboard(g3d_gui &g, bool firstpass)
    {
        s_sprintfd(modemapstr)("%s: %s", server::modename(gamemode), getclientmap()[0] ? getclientmap() : "[new map]");
        if((gamemode>1 || (gamemode==0 && (multiplayer(false) || demoplayback))) && minremain >= 0)
        {
            if(!minremain) s_strcat(modemapstr, ", intermission");
            else
            {
                s_sprintfd(timestr)(", %d %s remaining", minremain, minremain==1 ? "minute" : "minutes");
                s_strcat(modemapstr, timestr);
            }
        }
        g.text(modemapstr, 0xFFFF80, "server");
    
        int numgroups = groupplayers();
        loopk(numgroups)
        {
            if((k%2)==0) g.pushlist(); // horizontal
            
            scoregroup &sg = *groups[k];
            int bgcolor = sg.team && m_teammode ? (isteam(player1->team, sg.team) ? 0x3030C0 : 0xC03030) : 0,
                fgcolor = 0xFFFF80;

            g.pushlist(); // vertical
            g.pushlist(); // horizontal

            #define loopscoregroup(o, b) \
                loopv(sg.players) \
                { \
                    fpsent *o = sg.players[i]; \
                    b; \
                }    

            g.pushlist();
            if(sg.team && m_teammode)
            {
                g.pushlist();
                g.background(bgcolor, numgroups>1 ? 3 : 5);
                g.strut(1);
                g.poplist();
            }
            g.text("", 0, "server");
            loopscoregroup(o,
            {
                if(o==player1 && highlightscore && (multiplayer(false) || demoplayback))
                {
                    g.pushlist();
                    g.background(0x808080, numgroups>1 ? 3 : 5);
                }
                const playermodelinfo &mdl = getplayermodelinfo(o);
                const char *icon = sg.team && m_teammode ? (isteam(player1->team, sg.team) ? mdl.blueicon : mdl.redicon) : mdl.ffaicon;
                g.text("", 0, icon);
                if(o==player1 && highlightscore && (multiplayer(false) || demoplayback)) g.poplist();
            });
            g.poplist();

            if(sg.team && m_teammode)
            {
                g.pushlist(); // vertical

                if(sg.score>=10000) g.textf("%s: WIN", fgcolor, NULL, sg.team);
                else g.textf("%s: %d", fgcolor, NULL, sg.team, sg.score);

                g.pushlist(); // horizontal
            }

            if(!cmode || !cmode->hidefrags())
            { 
                g.pushlist();
                g.strut(7);
                g.text("frags", fgcolor);
                loopscoregroup(o, g.textf("%d", 0xFFFFDD, NULL, o->frags));
                g.poplist();
            }

            if(multiplayer(false) || demoplayback)
            {
                if(showpj)
                {
                    g.pushlist();
                    g.strut(6);
                    g.text("pj", fgcolor);
                    loopscoregroup(o,
                    {
                        if(o->state==CS_LAGGED) g.text("LAG", 0xFFFFDD);
                        else g.textf("%d", 0xFFFFDD, NULL, o->plag);
                    });
                    g.poplist();
                }
        
                if(showping)
                {
                    g.pushlist();
                    g.text("ping", fgcolor);
                    g.strut(6);
                    loopscoregroup(o, 
                    {
                        if(!showpj && o->state==CS_LAGGED) g.text("LAG", 0xFFFFDD);
                        else g.textf("%d", 0xFFFFDD, NULL, o->ping);
                    });
                    g.poplist();
                }
            }

            g.pushlist();
            g.text("name", fgcolor);
            loopscoregroup(o, 
            {
                int status = o->state!=CS_DEAD ? 0xFFFFDD : 0x606060;
                if(o->privilege)
                {
                    status = o->privilege>=PRIV_ADMIN ? 0xFF8000 : 0x40FF80;
                    if(o->state==CS_DEAD) status = (status>>1)&0x7F7F7F;
                }
                g.text(colorname(o), status);
            });
            g.poplist();

            if(showclientnum || player1->privilege>=PRIV_MASTER)
            {
                g.space(1);
                g.pushlist();
                g.text("cn", fgcolor);
                loopscoregroup(o, g.textf("%d", 0xFFFFDD, NULL, o->clientnum));
                g.poplist();
            }
            
            if(sg.team && m_teammode)
            {
                g.poplist(); // horizontal
                g.poplist(); // vertical
            }

            g.poplist(); // horizontal
            g.poplist(); // vertical

            if(k+1<numgroups && (k+1)%2) g.space(2);
            else g.poplist(); // horizontal
        }
        
        if(showspectators && spectators.length())
        {
            if(showclientnum || player1->privilege>=PRIV_MASTER)
            {
                g.pushlist();
                
                g.pushlist();
                g.text("spectator", 0xFFFF80, "server");
                loopv(spectators) 
                {
                    fpsent *o = spectators[i];
                    int status = 0xFFFFDD;
                    if(o->privilege) status = o->privilege>=PRIV_ADMIN ? 0xFF8000 : 0x40FF80;
                    if(o==player1 && highlightscore)
                    {
                        g.pushlist();
                        g.background(0x808080, 3);
                    }
                    g.text(colorname(o), status, "spectator");
                    if(o==player1 && highlightscore) g.poplist();
                }
                g.poplist();

                g.space(1);
                g.pushlist();
                g.text("cn", 0xFFFF80);
                loopv(spectators) g.textf("%d", 0xFFFFDD, NULL, spectators[i]->clientnum);
                g.poplist();

                g.poplist();
            }
            else
            {
                g.textf("%d spectator%s", 0xFFFF80, "server", spectators.length(), spectators.length()!=1 ? "s" : "");
                loopv(spectators)
                {
                    if((i%3)==0) 
                    {
                        g.pushlist();
                        g.text("", 0xFFFFDD, "spectator");
                    }
                    fpsent *o = spectators[i];
                    int status = 0xFFFFDD;
                    if(o->privilege) status = o->privilege>=PRIV_ADMIN ? 0xFF8000 : 0x40FF80;
                    if(o==player1 && highlightscore)
                    {
                        g.pushlist();
                        g.background(0x808080);
                    }
                    g.text(colorname(o), status);
                    if(o==player1 && highlightscore) g.poplist();
                    if(i+1<spectators.length() && (i+1)%3) g.space(1);
                    else g.poplist();
                }
            }
        }
    }

    struct scoreboardgui : g3d_callback
    {
        bool showing;
        vec menupos;
        int menustart;

        scoreboardgui() : showing(false) {}

        void show(bool on)
        {
            if(!showing && on)
            {
                menupos = menuinfrontofplayer();
                menustart = starttime();
            }
            showing = on;
        }

        void gui(g3d_gui &g, bool firstpass)
        {
            g.start(menustart, 0.03f, NULL, false);
            renderscoreboard(g, firstpass);
            g.end();
        }

        void render()
        {
            if(showing) g3d_addgui(this, menupos, scoreboard2d ? GUI_FORCE_2D : GUI_2D | GUI_FOLLOW);
        }

    } scoreboard;

    void g3d_gamemenus()
    {
        scoreboard.render();
    }

    VARFN(scoreboard, showscoreboard, 0, 0, 1, scoreboard.show(showscoreboard!=0));

    void showscores(bool on)
    {
        showscoreboard = on ? 1 : 0;
        scoreboard.show(on);
    }
    ICOMMAND(showscores, "D", (int *down), showscores(*down!=0));
}
