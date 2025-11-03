#!/usr/bin/env python3
"""
midi_clock.py

Genera MIDI Time Clock (0xF8) al BPM dado y envía por un puerto ALSA (rtmidi).
Instalación: pip install python-rtmidi
Uso ejemplo:
    python midi_clock.py --bpm 128 --port 1 --send-start-stop
    python midi_clock.py --bpm 120 --port-name "System MIDI" 
    python midi_clock.py --bpm 100   # abrirá puerto virtual si no se especifica puerto
"""

import argparse
import time
import sys
import rtmidi

MIDI_CLOCK = [0xF8]
MIDI_START = [0xFA]
MIDI_STOP  = [0xFC]
CLOCKS_PER_QUARTER = 24  # MIDI clock pulses per quarter note

def list_ports(midiout):
    ports = midiout.get_ports()
    if not ports:
        print("No MIDI output ports found.")
    else:
        print("Available MIDI output ports:")
        for i, p in enumerate(ports):
            print(f"  [{i}] {p}")
    return ports

def open_port_by_index(midiout, index):
    try:
        midiout.open_port(index)
        return True
    except Exception as e:
        print(f"Error abriendo puerto {index}: {e}")
        return False

def open_port_by_name(midiout, name):
    ports = midiout.get_ports()
    for i, p in enumerate(ports):
        if name in p:
            return open_port_by_index(midiout, i)
    print(f"No se encontró puerto con nombre que contenga: {name}")
    return False

def open_virtual(midiout, name="MIDI Clock Out (virtual)"):
    try:
        midiout.open_virtual_port(name)
        print(f"Abierto puerto virtual: {name}")
        return True
    except Exception as e:
        print(f"No se pudo crear puerto virtual: {e}")
        return False

def send_bytes(midiout, data):
    midiout.send_message(data)

def run_clock(midiout, bpm, send_start_stop=False):
    if bpm <= 0:
        raise ValueError("BPM debe ser > 0")
    interval = 60.0 / (bpm * CLOCKS_PER_QUARTER)  # segundos entre pulsos
    print(f"Running MIDI Clock at {bpm} BPM -> pulse interval: {interval*1000:.3f} ms")

    next_tick = time.monotonic()
    tick_count = 0

    if send_start_stop:
        print("Enviando START")
        send_bytes(midiout, MIDI_START)

    try:
        while True:
            # corregir deriva: calcular hora objetivo del siguiente pulso
            next_tick += interval
            # enviar el clock
            send_bytes(midiout, MIDI_CLOCK)
            tick_count += 1

            # dormir hasta next_tick (no negativa)
            now = time.monotonic()
            sleep_time = next_tick - now
            if sleep_time > 0:
                # usamos sleep corto; para alta precisión, podríamos hacer busy-wait (no recomendado)
                time.sleep(sleep_time)
            else:
                # If we are behind, adjust next_tick to now to avoid spiraling
                # (keeps sending as fast as necessary to catch up in worst case)
                next_tick = time.monotonic()
    except KeyboardInterrupt:
        print("\nInterrumpido por usuario")
        if send_start_stop:
            print("Enviando STOP")
            send_bytes(midiout, MIDI_STOP)
        print(f"Pulsos enviados: {tick_count}")

def main():
    parser = argparse.ArgumentParser(description="MIDI Time Clock generator (ALSA via rtmidi).")
    parser.add_argument("--bpm", type=float, default=120.0, help="BPM objetivo (default 120)")
    group = parser.add_mutually_exclusive_group()
    group.add_argument("--port", type=int, help="Índice de puerto MIDI de salida (ver --list-ports)")
    group.add_argument("--port-name", type=str, help="Parte del nombre del puerto para seleccionar")
    parser.add_argument("--list-ports", action="store_true", help="Listar puertos ALSA MIDI disponibles y salir")
    parser.add_argument("--send-start-stop", action="store_true", help="Enviar START (0xFA) al inicio y STOP (0xFC) al CTRL-C")
    parser.add_argument("--virtual-if-none", action="store_true", help="Abrir puerto virtual si no se especifica puerto y no hay puertos")
    args = parser.parse_args()

    midiout = rtmidi.MidiOut()
    ports = midiout.get_ports()

    if args.list_ports:
        list_ports(midiout)
        return

    opened = False
    if args.port is not None:
        if args.port < 0 or args.port >= max(1, len(ports)):
            print("Índice de puerto inválido.")
            list_ports(midiout)
            sys.exit(1)
        opened = open_port_by_index(midiout, args.port)
    elif args.port_name is not None:
        opened = open_port_by_name(midiout, args.port_name)
    else:
        # No port specified: if there are ports, ask user to choose (interactive) OR open virtual if requested
        if ports:
            print("No se especificó puerto. Se muestran puertos disponibles:")
            list_ports(midiout)
            try:
                sel = int(input("Selecciona índice de puerto (o -1 para crear puerto virtual): ").strip())
            except Exception:
                sel = None
            if sel == -1:
                opened = open_virtual(midiout)
            elif isinstance(sel, int) and 0 <= sel < len(ports):
                opened = open_port_by_index(midiout, sel)
            else:
                print("Selección inválida. Abortando.")
                sys.exit(1)
        else:
            if args.virtual_if_none:
                opened = open_virtual(midiout)
            else:
                print("No hay puertos MIDI y no se pidió puerto virtual. Usa --virtual-if-none o conecta un puerto ALSA.")
                sys.exit(1)

    if not opened:
        print("No se pudo abrir puerto MIDI. Saliendo.")
        sys.exit(1)

    try:
        run_clock(midiout, args.bpm, send_start_stop=args.send_start_stop)
    finally:
        try:
            midiout.close_port()
        except Exception:
            pass

if __name__ == "__main__":
    main()
