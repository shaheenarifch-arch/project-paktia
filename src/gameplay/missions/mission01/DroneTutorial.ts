export type TutorialStep = 'LAUNCH_RAVEN' | 'MOVE_RAVEN' | 'OBSERVE_CONTACT' | 'CONFIRM_CONTACT' | 'RETURN_RAVEN' | 'COMPLETE'
export class DroneTutorial {
  step: TutorialStep = 'LAUNCH_RAVEN'
  advance(completed: TutorialStep): TutorialStep {
    const order: TutorialStep[] = ['LAUNCH_RAVEN','MOVE_RAVEN','OBSERVE_CONTACT','CONFIRM_CONTACT','RETURN_RAVEN','COMPLETE']
    if (this.step !== completed) return this.step
    this.step = order[order.indexOf(completed) + 1] ?? 'COMPLETE'
    return this.step
  }
}
