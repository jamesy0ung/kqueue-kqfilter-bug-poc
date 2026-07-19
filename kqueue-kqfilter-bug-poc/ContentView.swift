//
//  ContentView.swift
//  kqueue-kqfilter-bug-poc
//
//  Created by James Young on 19/7/2026.
//

import SwiftUI

struct ContentView: View {
    @State private var status = "Idle"
    @State private var isRunning = false
    
    var body: some View {
        VStack(spacing: 16) {
            Text(status)
                .font(.system(.body, design: .monospaced))
                .multilineTextAlignment(.leading)
            Button(isRunning ? "Running…" : "Run kqueue cycle race") {
                runRace()
            }
            .disabled(isRunning)
        }
        .padding()
    }
    
    private func runRace() {
        isRunning = true
        status = "Racing…"
        DispatchQueue.global(qos: .userInitiated).async {
            let result = kq_race_run(200000)
            DispatchQueue.main.async {
                isRunning = false
                status = describe(result)
            }
        }
    }
    
    private func describe(_ r: kq_race_result_t) -> String {
        guard r.won == 1 else {
            return "race not won after \(r.iterations) iterations"
        }
        return "won at iteration \(r.iterations): A=\(r.kqa) B=\(r.kqb)\n"
        + "trigger result=\(r.trigger_result) errno=\(r.trigger_errno)"
    }
}

#Preview {
    ContentView()
}
