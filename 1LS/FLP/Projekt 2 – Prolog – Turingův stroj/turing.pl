% FLP Project 2: Turing Machine
% Author: Ondřej Ondryáš <xondry02@stud.fit.vutbr.cz>
% Date: 2023-04-27

:- dynamic rule/5. % rules are predicates rule(State, Symbol, NewState, NewSymbol, Action) created by reading the input
:- dynamic input/1. % input is a predicate input([Array of input chars as atoms])

% ==== Program ====
start :-
    %retractall(rule(_,_,_,_,_)),
    %retractall(input(_)),
    prompt(_, ''),
    read_input -> (
        turing(Output) -> (write_output(Output), halt)
        ; (write('No accepting sequence found.'), nl, halt(1))
    )
    ; (write('Invalid input.'), nl, halt(2)).

% ==== Input ====

valid_state([FirstCharCode|_]) :- % valid state: may be several chars long, first must be uppercase
    char_code(Char, FirstCharCode),
    char_type(Char, upper).

valid_sym([32]). % valid symbol: blank (space)
valid_sym([C]) :- % valid symbol: one lowercase char
    char_code(Char, C),
    char_type(Char, lower).

valid_input([], []). % valid input: string of lowercase characters or spaces
valid_input([C|T], [Atom|T2]) :- 
    char_code(Char, C),
    (char_type(Char, lower); Char = ' '),
    atom_codes(Atom, [C]),
    valid_input(T, T2).

read_input :-
    read_line_to_codes(user_input, RawIn), % reads the line as list of character codes
    % if input is not EOF
    (   RawIn \== end_of_file
    % then
    ->      (   split(RawIn, CharParts), % split the list by spaces (code 32) into a list of sublists
                % if the list contains sublists with definitions of states and symbols
                (   CharParts = [State, Sym, NewState, SymOrAction] % each of the variables gets bound to a list
                % then
                ->  make_rule(State, Sym, NewState, SymOrAction),
                    read_input
                % else if the list contains no spaces
                ;   (	CharParts = [Input] 
                    -> make_input(Input), !
                    ;   !, fail
                    )
                )
                % end if
            )
    % else fail
	;   !, fail).
	% end if

%! make_rule(+State:list, +Sym:list, +NewState:list, +SymOrAction:list) is det.
% The input parameters are lists of character codes.
% The states and symbols are internally represented as atoms.
make_rule(State, Sym, NewState, SymOrAction) :-
    valid_state(State),
    valid_state(NewState),
    valid_sym(Sym),
    % convert lists of character codes to atoms
    atom_codes(StateAt, State),
    atom_codes(SymAt, Sym),
    atom_codes(NewStateAt, NewState),
    % SymOrAction is not validated nor converted here because we use unification 
    % with make_rule_int head to distinguish between L/R 'actions' and symbols
	make_rule_int(StateAt, SymAt, NewStateAt, SymOrAction).    
    
make_rule_int(State, Sym, NewState, [76]) :- % 76 is L
    assert(rule(State, Sym, NewState, Sym, left)).
make_rule_int(State, Sym, NewState, [82]) :- % 82 is R
    assert(rule(State, Sym, NewState, Sym, right)).
make_rule_int(State, Sym, NewState, NewSymStr) :-
    valid_sym(NewSymStr),
    atom_codes(NewSym, NewSymStr),
    assert(rule(State, Sym, NewState, NewSym, stay)).
    
make_input(In) :-
    valid_input(In, Atoms),
    assert(input(Atoms)).

take_until([],       _, [],     []) :- !.  % hit the end of the source list
take_until([X|Xs],   X, [],     Xs) :- !.  % hit a space
take_until([X|Xs],   S, [X|Ps], Rs) :-     % head is not a space
  	X \= S,
  	take_until(Xs , S , Ps , Rs).

split(L, [A,B,C,D]) :-
  	take_until(L, 32, A, [R,32|Rs]), % 32 is space
  	B=[R],
    take_until(Rs, 32, C, [P]),
    D=[P], !.

split(L, [L]).

write_output([]).
write_output([H|T]) :-
    writeln(H),
    write_output(T).

% ==== Machine ====

/**
 * config(-StartState:atom, -FinalState:atom, -Blank:atom) is det.
 *
 * Specifies what atoms represent the initial and final states 
 * of the Turing machine and the blank symbol.
 */
config(StartState, FinalState, Blank) :-
    StartState  = 'S',
    FinalState  = 'F',
    Blank       = ' '.

/**
 * turing(-Configurations:list) is det.
 *
 * Simulates a Turing machine given by config() on the input given by input().
 *
 * @param Configurations Output variable will be bound to a list that contains string 
 *        representation of the accepting chain of configurations.
 */
turing(Configurations) :-
    config(IS, _, _),
    input(TapeIn),
    tm_step(IS, [], TapeIn, _, _, [], Buffer), !,
    reverse(Buffer, Configurations).

print_list([]).
print_list([H|T]) :- write(H), print_list(T).
print_conf(L, State, R) :- print_list(L), write(State), print_list(R).

/**
 * append_conf_to_buffer(+L:list, +State:atom, +R:list, +InBuf:list, -OutBuf:list) is det.
 *
 * Adds a configuration to the output buffer.
 *
 * @param L List representing the contents of the tape before the head (actually contains the reverse).
 * @param State Atom representing the current state of the Turing machine
 * @param R List that contains the contents of the tape after the head.
 * @param InBuf The previous list of configurations.
 * @param OutBuf The new list of configurations.
 */
append_conf_to_buffer(L, State, R, InBuf, OutBuf) :-
    reverse(L, LRev),
    with_output_to(string(S), print_conf(LRev, State, R)),
    append(InBuf, [S], OutBuf).

/**
 * tm_step(+State:atom, +LIn:list, +RIn:list, -LOut:list, -ROut:list, +InBuf:list, -OutBuf:list) is det.
 *
 * Executes a single step of the Turing machine.
 *
 * @param State Atom representing the current state of the Turing machine.
 * @param LIn List representing the contents of the tape before the head (actually contains the reverse).
 * @param RIn List that contains the contents of the tape after the head.
 * @param LOut Output variable that will contain the new contents of the tape before the head.
 * @param ROut Output variable that will contain the new contents of the tape after the head.
 * @param InBuf The previous list of configurations.
 * @param OutBuf The new list of configurations.
 */
tm_step(FS, L, R, L, R, InBuf, OutBuf) :- config(_, FS, _), append_conf_to_buffer(L, FS, R, InBuf, OutBuf).
tm_step(State, LIn, RIn, LOut, ROut, InBuf, OutBuf) :-
    config(_, _, B),
    symbol(RIn, Symbol, Rs, B),
    rule(State, Symbol, NewState, NewSymbol, Action),
    move_head(Action, LIn, [NewSymbol|Rs], LNext, RNext, B),
    tm_step(NewState, LNext, RNext, LOut, ROut, InBuf, NextOutBuf),
    
    append_conf_to_buffer(LIn, State, RIn, NextOutBuf, OutBuf). 

/**
 * symbol(+TapeIn:list, -Symbol:atom, -RightRem:list, +Blank:atom)
 * 
 * Extracts the current symbol from the right side of the input tape. If there are no
 * symbols left to extract, a blank symbol is returned instead.
 * 
 * @param TapeIn The input tape represented as a list.
 * @param Symbol The current symbol under the tape head.
 * @param RightRem The remaining symbols to the right of the tape head.
 * @param Blank The atom representing the blank symbol of the Turing machine.
 */
symbol([],       B,   [], B).
symbol([Sym|Rs], Sym, Rs, _).

/**
 * move_head(+Action:atom, +LIn:list, +RIn:list, -LOut:list, -ROut:list, +Blank:atom)
 * 
 * Modifies the tape according to the current state and symbol under the tape head. 
 * If the action is 'left' and the tape head is at the leftmost end of the tape, 
 * a blank symbol is added to the left of the tape. If the action is 'right' and
 * the tape head is at the rightmost end of the tape, a blank symbol is added to 
 * the right of the tape. If the action is 'stay', no changes to the tape are done
 * (the symbol is already rewritten in RIn).
 * 
 * Note that the list representing the left part of the tape actually contains the reverse
 * of the tape (so that we can always work just with the head).
 * 
 * @param Action The action to be taken by the Turing machine.
 * @param LIn List representing the current contents of the tape before the head (actually contains the reverse).
 * @param RIn  List that contains the contents of the tape after the head.
 * @param LOut The new left side of the tape.
 * @param ROut The new right side of the tape.
 * @param Blank The atom representing the blank symbol of the Turing machine.
 */
move_head(left,  [],     RIn,    [],    [B|RIn], B). % move left, left part empty -> add blank to beg. of right
move_head(left,  [L|Ls], RIn,    Ls,    [L|RIn], _). % move left, left part non-empty -> "end" of left part moves to right
move_head(right, L,      [],     [B|L], [],      B). % move right, right part empty -> add blank to "end" of left part
move_head(right, L,      [R|Rs], [R|L], Rs,      _). % move right, right part non-empty -> beginning of right moves to "end" of left
move_head(stay,  L,      R,      L,     R,       _). % stay -> no symbols move
